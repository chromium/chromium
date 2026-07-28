# Kombucha Interactive Testing Playbook

[go/kombucha-playbook](https://goto.google.com/kombucha-playbook)

See also: [API overview](./README.md)

# Overview {#overview}

Kombucha is a paradigm for writing interaction tests in Chrome. It is a simple, framework-agnostic, gray-box testing framework implemented in C++ (with JavaScript support) that lives in the same codebase as existing Chrome browser and interaction tests.

[TOC]

## The Basics {#the-basics}

**InteractiveBrowserTest**

* Replaces InProcessBrowserTest \- derive your test fixture from it instead
* Entry point is RunTestSequence()
* Sequence of steps that refer to UI Elements

**ui::TrackedElement**

* Basic unit of UI
* Can be referred to by ElementIdentifier or a name you assign during the test

**Verbs**

* Test actions you can take, e.g. “PressButton”
* Test checks of expected conditions, e.g. “CheckElement”
* Events that allow the test to advance, e.g. “WaitForEvent”

---

## Getting Started {#getting-started}

In this section a few examples of a test, starting with a very simple action sequence, and then extending into more complex tests.

Common steps of a test look like setting up a specific state in the UI, interacting with the UI, and verifying a state or status in the UI. See verbs [documentation](https://chromium.googlesource.com/chromium/src/+/main/chrome/test/interaction/README.md) ([go/kombucha-api](http://goto.google.com/kombucha-api)).

We also have [our slide deck from the 2022 Kombucha fixit](http://goto.google.com/kombucha-fixit-2022) which provides an introduction to writing Kombucha tests, with examples of each type of verb.

### Basic Sample {#basic-sample}

In this sample, we verify we can click the app menu button and then select the “Downloads” menu item:

```cpp
class MyTest : public InteractiveBrowserTest { ... };

IN_PROC_BROWSER_TEST_F(MyTest, TestName) {
  RunTestSequence(
      PressButton(kAppMenuButtonElementId),
      SelectMenuItem(kDownloadsAppMenuItemElementId)
  );
}
```

### Next Steps: Verifying Browser Changes {#next-steps:-verifying-browser-changes}

In this follow-up, we take the previous test and add a check that the Downloads menu item has the correct text, and also that the Downloads page opens.

```cpp
class MyTest : public InteractiveBrowserTest { ... };

IN_PROC_BROWSER_TEST_F(MyTest, TestName) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);

  RunTestSequence(
      PressButton(kAppMenuButtonElementId),

      // Wait for the downloads item to appear and check its title.
      WaitForShow(kDownloadsAppMenuItem),
      CheckViewProperty(
          kDownloadsAppMenuItem,
          &MenuItemView::title,
          l10n_util::GetStringUTF16(IDS_SHOW_DOWNLOADS)),

      // Watch for a new tab, select the “downloads” item, and ensure the
      // Downloads page loads in the new tab.
      InstrumentNextTab(kPrimaryTabId),
      SelectMenuItem(kDownloadsAppMenuItemElementId),
      WaitForWebContentsReady(kNewTabId, GURL("chrome://downloads"))
  );
}
```

### Advanced: Probing Web Page Contents {#advanced:-probing-web-page-contents}

Of course, if we actually want to test the Downloads page, we can go further. In this example, we open the Downloads page and verify that we can open the menu on the page and clear our downloads (note that this was from the old page layout; there is no longer a popup menu).

```cpp
class MyTest : public InteractiveBrowserTest { ... };

IN_PROC_BROWSER_TEST_F(MyTest, TestName) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);

  // Javascript function to click an element.
  constexpr char kClickFn[] = "el => el.click()";

  // This specifies the sequence of Polymer elements required to locate the
  // "more actions" button in the Downloads page.
  const DeepQuery kPathToDownloadsPageMenu{
    "downloads-manager",
    "downloads-toolbar",
    "#toolbar",
    "#moreActions"
  };

  // Description of state change when the “clear downloads” menu item becomes
  // visible. This is done via `getClientBoundingRect()` since the parent menu
  // is collapsed when it is not visible, so this is the most reliable way to
  // test visibility.
  const DeepQuery kPathToClearDownloadsMenuItem{
    "downloads-manager",
    "downloads-toolbar",
    "#moreActionsMenu",
    ".clearAll"
  };
  StateChange clear_downloads_visible;
  clear_downloads_visible.where = kPathToClearDownloadsMenuItem;
  clear_downloads_visible.test_function =
      "el => el.getClientBoundingRect().height > 0";

  // Description of the state change when the downloads list is empty.
  // When this happens, the “no-downloads” element becomes visible.
  const DeepQuery kPathToNoDownloadsVisible{
    "downloads-manager",
    "#no-downloads:not([hidden])"
  };
  StateChange downloads_list_empty;
  downloads_list_empty.type = StateChange::Type::kExists;
  downloads_list_empty.where = kPathToNoDownloadsVisible;

  RunTestSequence(
      PressButton(kAppMenuButtonElementId),

      // Watch for a new tab, select the “downloads” item, and ensure the
      // Downloads page loads in the new tab.
      InstrumentNextTab(kPrimaryTabId),
      SelectMenuItem(kDownloadsAppMenuItemElementId),
      WaitForWebContentsReady(kPrimaryTabId, GURL("chrome://downloads"))

      // Click the Downloads page “more actions” button.
      ExecuteJsAt(kPrimaryTabId, kPathToDownloadsPageMenu, kClickFn),
      WaitForStateChange(kPrimaryTabId, clear_downloads_visible),
      ExecuteJsAt(kPrimaryTabId, kPathToClearDownloadsMenuItem, kClickFn),

      // Wait for the "no downloads" image to appear.
      WaitForStateChange(kPrimaryTabId, downloads_list_empty)
  );
}
```

There are a lot of different ways we could have verified that the downloads page had been cleared \- looking for no visible download items, looking for the “no downloads” element to become visible, etc. Test as many as you like; **WaitForStateChange** will succeed if the desired state is already present.

You will still notice that the actual test sequence is quite terse; it’s only eight steps and quite readable.

Also notice that it’s possible for a test to fail if the structure of a page changes; this is by design. If a page’s structure or function changes, regression tests *should* be updated to match the new functionality. And in the case that it is purely an organizational change, the update would simply be modifying a couple of queries to reflect the new paths to the elements in question.

### Advanced: Waiting for Non-UI State {#advanced:-waiting-for-non-ui-state}

Kombucha now allows you to observe system state in tests without having to go through a UI element or use a custom event. (Custom events are still great for discrete events that you want to respond to in real-time though\!)

To do this, you must derive an observer from **ui::test::StateObserver** or (if you are observing an object that uses our standard “observer” pattern) from **ui::test::ObservationStateObserver**. The observer should:

* Call **OnStateObserverStateChanged()** when the state changes.
* Optionally, override **GetStateObserverInitialState()**.
* Optionally, if you use **ObservationStateObserver**, call **OnObservationStateObserverSourceDestroyed()** when the object you are observing is destroyed.

```cpp
class SystemCountObserver
  : public ui::test::ObservationStateObserver<int, System, SystemObserver> {
 public:
  SystemCountObserver(System* system) : ObservationStateObserver(system) {}
  ~SystemCountObserver() override = default;

  // StateObserver:
  int GetStateObserverInitialState() const override {
    return source()->count();
  }

  // MySystemObserver:
  void OnSystemCountChanged(System*, int count) {
    OnStateObserverStateChanged(count);
  }
  void OnSystemDestroying(System*) {
    OnObservationStateObserverSourceDestroyed();
  }
};
```

Then, in your test file, declare a state identifier you will use in your test, and use **ObserveState()** and **WaitForState()**. Note that the type of the observer is built into the identifier, so you don’t have to specify it again, when you call **ObserveState()**. However, you can also explicitly create a **unique\_ptr** of your observer and pass that in instead of the constructor arguments.

```cpp
IN_PROC_BROWSER_TEST_F(SystemTest, CountIncrementsAfterAsyncAction) {
  // This creates a StateIdentifier and associates it with our observer class.
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(SystemCountObserver, kCountId);
  System* const system = GetSystem();
  RunTestSequence(
    // This will automatically construct a SystemCountObserver; additional
    // arguments are forwarded to the constructor.
    ObserveState(kCountId, system),
    // Verify the initial count.
    WaitForState(kCountId, 0),
    // Start the asynchronous operation.
    PressButton(kPerformAsyncSystemActionButtonId),
    // Verify the operation completes and the count is incremented.
    WaitForState(kCountId, 1));
}
```

#### Polling State {#polling-state}

If there is a system you would like to observe without the ability to register for callbacks/add observer objects, you can still poll the state using **PollState** or the related verbs **PollElement** and **PollViewProperty**. Note that in this case you do *not* need to create a new **StateObserver**. In this case, you simply provide a method or function that generates the state, and it will be checked periodically.

```cpp
IN_PROC_BROWSER_TEST_F(SystemTest, CountIncrementsAfterAsyncAction) {
  // This creates a StateIdentifier and associates it with our observer class.
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(PollingStateObserver<int>, kCountId);
  System* const system = GetSystem();
  RunTestSequence(
    // This will automatically construct a SystemCountObserver; additional
    // arguments are forwarded to the constructor.
    PollState(kCountId, [system](){ return system->GetCount(); }),
    // Verify the initial count.
    WaitForState(kCountId, 0),
    // Start the asynchronous operation.
    PressButton(kPerformAsyncSystemActionButtonId),
    // Verify the operation completes and the count is incremented.
    WaitForState(kCountId, 1));
}
```

One caveat with **Poll\*** verbs is that if the state is *transient* (that is, it changes to multiple values in rapid succession) you may miss the value you want to wait for. This is why **ObserveState** is more robust than **PollState**.

#### Case Study: Translation {#case-study:-translation}

Consider the following logic for a test that verifies that page translation works. Here’s how the test was initially written, with the “wait for page to translate” code completely outside the test::

```cpp
class TranslateBubbleViewUiTest : public InteractiveBrowserTest {

  // ...

  void NavigateAndWaitForLanguageDetection(
      const GURL& url, const std::string& expected_lang) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    while (expected_lang !=
           ChromeTranslateClient::FromWebContents(
               browser()->tab_strip_model()->GetActiveWebContents())
               ->GetLanguageState()
               .source_language()) {
      CreateTranslateWaiter(
          browser()->tab_strip_model()->GetActiveWebContents(),
          TranslateWaiter::WaitEvent::kLanguageDetermined)
              ->Wait();
    }
  }
};

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewUiTest, SomeTest) {
  // P1.Opened/Navigate to non english page > Hit on Translate bubble icon.
  const GURL french_url =
      GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  RunTestSequence(...);
}
```

On first glance, it may not be possible to eliminate the **TranslateWaiter**, but this could still be expressed using Kombucha verbs, with a helper function that does the waiting:

```cpp
class TranslateBubbleViewUiTest : public InteractiveBrowserTest {

  // ...

  void WaitForExpectedLanguage(const std::string& expected_lang) {
    while (expected_lang !=
           ChromeTranslateClient::FromWebContents(
               browser()->tab_strip_model()->GetActiveWebContents())
               ->GetLanguageState()
               .source_language()) {
      CreateTranslateWaiter(
          browser()->tab_strip_model()->GetActiveWebContents(),
          TranslateWaiter::WaitEvent::kLanguageDetermined)
          ->Wait();
    }
  }

  auto NavigateAndWaitForLanguageDetection(
      ElementIdentifier page_id,
      const GURL& gurl, const std::string& expected_lang) {
    auto steps = Steps(
      NavigateWebContents(page_id, gurl),
      Do(base::BindOnce(
          &TranslateBubbleViewUiTest::WaitForExpectedLanguage
          base::Unretained(this), expected_lang))
      AddDescriptionPrefix(steps, "NavigateAndWaitForLanguageDetection");
      return steps;
    );
  }
};

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewUiTest, SomeTest) {
  const GURL french_url =
      GURL(embedded_test_server()->GetURL("/french_page.html"));

  RunTestSequence(
    InstrumentTab(kPrimaryTabId),
    NativateAndWaitForLanguageDetection(kPrimaryTabId, french_url, "fr"),
    // ...
  );
}
```

However, perhaps we can observe the translation service or have it emit an event separately ([see the FAQ entry on dealing with non-Kombucha events](#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?)). In this case, since we might already be in the desired state, an **If** directive is also warranted:

```cpp
class TranslateBubbleViewUiTest : public InteractiveBrowserTest {

  // ...

  // Returns a OnceCallback that gets the current source language.
  auto GetSourceLanguage() {
    return base::BindLambdaForTesting([this]() {
      auto* const client = ChromeTranslateClient::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
      return client->GetLanguageState().source_language();
    });
  }

  // Navigates `page_id` (which should be the active tab) to `gurl` and
  // waits for the source language to be detected and match `expected_lang`.
  auto NavigateAndWaitForLanguageDetection(
      ElementIdentifier page_id,
      const GURL& gurl, const std::string& expected_lang) {
    auto steps = Steps(
      NavigateWebContents(page_id, gurl),
      // If the detected language isn’t already the expected one, wait for
      // detection to complete.
      IfMatches(GetSourceLanguage(), testing::Ne(expected_lang),
         Then(
           // Assume that the event is relayed through the browser.
           WaitForEvent(kBrowserViewElementId,
                        kTranslateLanguageDetectedEvent)
           // Verify the correct language was detected.
           CheckResult(GetSourceLanguage(), expected_lang))
      ),
    );
    AddDescriptionPrefix(steps, "NavigateAndWaitForLanguageDetection");
    return steps;
  }
};

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewUiTest, SomeTest) {
  const GURL french_url =
      GURL(embedded_test_server()->GetURL("/french_page.html"));

  RunTestSequence(
    InstrumentTab(kPrimaryTabId),
    NativateAndWaitForLanguageDetection(kPrimaryTabId, french_url, “fr”),
    // ...
  );
}
```

This is not as concise in terms of lines of code, but much more clearly describes what is happening, and won’t fail or time out unexpectedly if a condition isn’t met (it will explicitly fail with a useful error message).

Finally, if you can observe the state directly, you can create a **StateObserver**, perhaps with the ability to track the detected language. This would look something like:

```cpp
class DetectedLanguageObserver
    : public StateObserver<std::string>,
      public TranslateDriver::LanguageDetectionObserver {
  // Read initial state from `client`, but observe the TranslateDriver.
  explicit DetectedLanguageObserver(ChromeTranslateClient* client);
  ~DetectedLanguageObserver() override;

  // Overrides for both StateObserver and LanguageDetectionObserver go here...
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(DetectedLanguageObserver,
                                    kDetectedLanguageState);

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewUiTest, SomeTest) {
  const GURL french_url =
      GURL(embedded_test_server()->GetURL("/french_page.html"));

  RunTestSequence(
    ObserveState(kDetectedLanguageState, [this](){
      return ChromeTranslateClient::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
    }),
    InstrumentTab(kPrimaryTabId),
    NavigateWebContents(kPrimaryTabId, french_url),
    WaitForState(kDetectedLanguageState, "fr"));
  );
}
```

As you can see, this is *much* more concise, but you will need to implement a **StateObserver** to connect the system events with your test. Note that you cannot use an **ObservationStateObserver** because **TranslateDriver** and **TranslateDriver::LanguageDetectionObserver** do not follow the normal observer naming conventions\! (In most cases, they will, and that will save you at least some boilerplate.)

Finally, you can use **PollState** for an even more concise test; this is especially useful if there’s no easy way to observe the value changing:

```cpp
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(PollingStateObserver<std::string>,
                                    kDetectedLanguageState);

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewUiTest, SomeTest) {
  const GURL french_url =
      GURL(embedded_test_server()->GetURL("/french_page.html"));

  RunTestSequence(
    PollState(kDetectedLanguageState, [this](){
      return ChromeTranslateClient::FromWebContents(
               browser()->tab_strip_model()->GetActiveWebContents())
               ->GetLanguageState()
               .source_language();
    }),
    InstrumentTab(kPrimaryTabId),
    NavigateWebContents(kPrimaryTabId, french_url),
    WaitForState(kDetectedLanguageState, "fr"));
  );
}
```

---

## Best Practices {#best-practices}

Here are a few general best practices for test organization.

### Setup {#setup}

Don’t attempt to do any checking of the UI in your setup; that’s what RunTestSequence is for. You can, however, set up any common state that is required by all tests in your fixture, performing necessary sanity checks to ensure you are in the proper state to start each test.

If you do need to do specific UI interaction or validation as a preamble to every test, consider creating a [*custom verb*](#custom-verbs) instead, and including that at the start of each test sequence.

### Callbacks {#callbacks}

Many verbs (such as **Do**, **Check**, **WithElement**, **AfterShow**, etc.) expect a lambda or function callback that actually does the work. You can always pass a callback to these verbs. However, for the vast majority of them, you do not need to use **BindOnce** unless you are explicitly binding arguments; you will almost never need to use **BindLambdaForTesting**. (This didn’t used to be the case, so you may see existing code with explicit bind calls.)

We recommend only using **BindOnce**/**BindRepeating**/**BindLambdaForTesting** if:

* The verb explicitly requires it.
* You need to bind parameters (but note you can capture them by value or reference in a lambda expression instead).
* You are creating a callback generator that will be used in multiple tests; by convention this should be an **auto** method that returns a callback.

Many of these verbs also allow you to omit some or all arguments, starting from the left side of the argument list. *It is preferred to omit unnecessary arguments wherever possible*.

Example:

```cpp
// By convention, shared callback generators use BindOnce or
// BindLambdaForTesting and return auto.
auto GetHistogramCount(const char* name) {
  return base::BindOnce(
      [](std::string name){
        return GetHistogramCount(name);
      }, name);
}

// This function’s signature is already compatible with verbs like WithView, so
// it can be passed directly.
static void ManipulateView(MyViewType* view) { ... }

RunTestSequence(
  // Can pass a lambda with state directly as a callback; also note that both
  // arguments are omitted. No need to call BindLambdaForTesting.
  AfterShow(kMyViewId, [this](){ InitializeSystem(); }),
  // This check uses a shared callback generator.
  CheckResult(CheckHistogramCount(kMyHistogramName), 0),
  // This uses a pointer to a compatible function. Again, no Bind is needed.
  WithView(&ManipulateView),
  CheckResult(CheckHistogramCount(kMyHistogramName), 1));
```

Note that instead of “GetHistogramCount(name)”, you could just as easily have created a custom verb “CheckHistogramCount(name, count)” that returns the entire **CheckResult** step.

Remember: a goal of Kombucha is to absolutely minimize boilerplate in tests. So try to make your test code as concise as possible without sacrificing readability\!

### Custom Verbs {#custom-verbs}

Custom verbs are ways to share common functionality between tests. By convention, a custom verb is a function on your test fixture with return type **auto** that returns either a **StepBuilder** or **MultiStep**. Custom verbs should also use `SetDescriptionPrefix()` to provide debugging context for their constituent sub-steps. This section has some examples of using custom verbs for shared steps as well as setup.

#### Breaking out Common Functionality As a Custom Verb {#breaking-out-common-functionality-as-a-custom-verb}

Here is a test sequence that has some steps that could be shared between tests:

```cpp
RunTestSequence(
  InstrumentTab(kPrimaryTabId),

  // Navigate to a SRP URL and then once to a non-SRP URL.
  NavigateWebContents(kPrimaryTabId, srp_url),
  NavigateWebContents(kPrimaryTabId, non_srp_url_1),
  // Ensure that the side search button is present, but the side search
  // panel isn't open.
  WaitForShow(kSideSearchButtonElementId),
  EnsureNotPresent(kSidePanelElementId),
  // Going back will increase the returned-to-SRP count to 1.
  PressButton(kBackButtonElementId),
  WaitForWebContentsNavigation(kPrimaryTabId),

  // The side panel should not automatically open when navigating to a
  // non-SRP URL.
  NavigateWebContents(kPrimaryTabId, non_srp_url_2),
  WaitForShow(kSideSearchButtonElementId),
  EnsureNotPresent(kSidePanelElementId),
  // Going back will increase the returned-to-SRP count to 2.
  PressButton(kBackButtonElementId),
  WaitForWebContentsNavigation(kPrimaryTabId),
  // ...
```

Here’s a custom verb in test fixture class that generates the common setup steps.
By convention, custom verbs return **auto**. Custom verbs should also use
`AddDescriptionPrefix()` to ensure that if a step inside the custom verb fails,
it will be easy to identify the exact step causing the issue.

```cpp
class MyTestFixture : public InteractiveBrowserTest {

  // ...

  // Create a state where side-search is primed to trigger but has not
  // triggered yet. Navigates in `page_id`.
  auto PrimeSideSearchToRun(
      ElementIdentifier page_id, GURL srp_url, GURL non_srp_url) {
    auto steps = Steps(
      // Navigate to a SRP URL and then once to a non-SRP URL.
      NavigateWebContents(page_id, srp_url),
      NavigateWebContents(page_id, non_srp_url),
      // Ensure that the side search button is present, but the side search
      // panel isn't open.
      WaitForShow(kSideSearchButtonElementId),
      EnsureNotPresent(kSidePanelElementId),
      // Going back will increase the returned-to-SRP count to 1.
      PressButton(kBackButtonElementId),
      WaitForWebContentsNavigation(page_id));
    AddDescriptionPrefix(steps, "PrimeSideSearchToRun");
    return steps;
  }
```

And here is the updated test body using the shared verb:

```cpp
RunTestSequence(
  InstrumentTab(kPrimaryTabId),

  // Call our custom verb to prepare side-search to run.
  PrimeSideSearchToRun(kPrimaryTabId, srp_url, non_srp_url_1),

  // The side panel should not automatically open when navigating to a
  // non-SRP URL.
  NavigateWebContents(kPrimaryTabId, non_srp_url_2),
  WaitForShow(kSideSearchButtonElementId),
  EnsureNotPresent(kSidePanelElementId),
  // Going back will increase the returned-to-SRP count to 2.
  PressButton(kBackButtonElementId),
  WaitForWebContentsNavigation(kPrimaryTabId),
  // ...
```

The **PrimeSideSearchToRun** custom verb can now be used in any test that wants to set up this condition.

#### Nested Steps() {#nested-steps()}

Note that since the arguments to **Steps** can be either **StepBuilders** or **MultiStep**s, you can nest calls to **Steps** within a **Steps** block. This isn’t particularly useful unless you need to selectively add items to a sequence in a custom verb; see [the section on conditional test steps](#can-i-perform-test-steps-conditionally?) for more information:

```cpp
auto CustomVerb(bool include_extra_steps) {
  auto steps = Steps(
    // step before conditional
    include_extra_steps ?
        Steps(/* extra steps to include*/) :
        MultiStep(),
    // steps after conditional
  );
  AddDescriptionPrefix(steps, "CustomVerb");
  return steps;
}
```

#### Nested Custom Verbs {#nested-custom-verbs}

Custom verbs, being step generators, can be used by other custom verbs:

```cpp
auto CustomVerb1() {
  auto steps = Steps(...);
  AddDescriptionPrefix(steps, "CustomVerb1");
  return steps;
}

auto CustomVerb2() {
  auto steps = Steps(
    // ...
    CustomVerb1(),
    // ...
  );
  AddDescriptionPrefix(steps, "CustomVerb2");
  return steps;
}
```

### The Interact-Observe-Verify Pattern {#the-interact-observe-verify-pattern}

Say you want to make some changes in a dialog and then confirm those changes, and finally, you want to check the underlying model to determine if the changes were properly committed.

You might consider the following code:

```cpp
RunTestSequence(
  PopUpMyDialog(),
  PressButton(MyDialog::kIncreaseTheThingButtonElementId),
  PressButton(MyDialog::kConfirmButtonElementId),
  // This is bad! We'll explain why below!
  CheckResult([this](){ model()->count(); }, 2, "Check model count."),
```

The problem here is, it’s going to press the button and immediately check the model. What if the changes to the model are not committed until after the dialog closes, and that happens asynchronously? What if clicking the button doesn’t work for some reason \- there will be no way to know that; we’ll just get the wrong number.

**We don’t want to make assumptions about how the underlying code is implemented when writing Kombucha tests**, because the underlying implementation might change without the user journey changing\! This would create a test that would be harder to maintain in the long-term.

Remember that Kombucha is, first and foremost, an *interaction testing* library. So the first thing to do after an interaction is to verify the result by observing the UI change; the model can be verified afterwards. We call this the “interact-observe-verify” pattern.

```cpp
RunTestSequence(
  PopUpMyDialog(),
  PressButton(MyDialog::kIncreaseTheThingButtonElementId),

  // INTERACT with the dialog.
  PressButton(MyDialog::kConfirmButtonElementId),

  // OBSERVE the dialog closing.
  WaitForHide(MyDialog::kDialogElementId),

  // VERIFY the model change.
  CheckResult([this](){ model()->count(); }, 2, "Check model count."),
```

As you can see here, the test *observes* the dialog closing, then *verifies* the update to the model. If the dialog fails to commit its changes and close, then the **WaitForHide** step will fail. If the model is not correctly updated, then the **CheckResult** step will fail instead.

#### Verifying Transient Objects and States: **WithoutDelay** {#verifying-transient-objects-and-states:-withoutdelay}

Sometimes the thing you want to verify changes or goes away with the element you are observing \- for example, a controller that is deleted when a dialog closes. Normally, you would observe the dialog closing, then check the controller. But if the controller is deleted with the dialog, you have a problem: [Kombucha is asynchronous](https://docs.google.com/document/u/0/d/1aizUj98Tkf6jZIKuIhnZBtRzCT24gu0tnLSf4_dZdnU/edit). A step triggers, then the callback or action or check is done *on a fresh call stack* via the message pump. Stuff can happen in the interim that can mess with your test \- like an object you want to check being deleted.

This choice to make Kombucha asynchronous actually removes a *lot* of race conditions, order-of-operations bugs, and other issues that previously plagued Kombucha tests and required the random insertion of “FlushEvents()” calls. However, it can introduce new bugs in very specific cases. In these cases, it might be necessary to force one or more steps to run as fast as possible, ideally on the same call stack, in response to a single trigger.

To do this, use **WithoutDelay**. Wrap the verbs you want to run together in a **WithoutDelay** and, starting with the first step, the callback or action or check will run immediately when the step is triggered, followed by \- if the conditions are met \- the steps will also execute immediately. Here’s an example with a dialog closing \- all three of the final steps can fully execute inside the call stack of pressing the button and its associated action:

```cpp
DialogController* controller = nullptr;
RunTestSequence(
  OpenDialog(),
  // Capture the controller. This will be valid as long as the dialog is open.
  WithView(kDialogElementId, [&](MyDialog* dialog) {
    controller = dialog->controller();
  })
  CheckResult([&](){ return controller->count(); }, 0),
  PressButton(kIncrementButtonId),
  // Closing the dialog destroys the controller, so these steps need to all be
  // executed together:
  WithoutDelay(
      PressButton(kCommitButtonId),
      WaitForHide(kDialogElementId),
      CheckResult([&](){ return controller->count(); }, 1)));
```

**WithoutDelay** can also be abused to hide volatility or bugginess in underlying systems, but you should avoid this whenever possible \- you are likely to hide actual bugs and/or introduce flakiness into your test this way.

### Describe Your Steps {#describe-your-steps}

By default, a step’s description is just the low-level Kombucha verb. Especially when creating custom verbs, it can be beneficial to add additional description to tests; this will be especially important when reading [test log output](#test-log-output,-what-if-my-test-fails?) to diagnose failures.

* **Step::SetDescription()** replaces the step’s description with text of your choosing.
* **Step::FormatDescription()** wraps the original description in a format string; the first “%s” in the format string receives the original description.
* **AddDescription(steps, format\_string)** updates all of the steps in *steps* by calling **FormatDescription()** with the provided *format\_string*.
* **Check** and **CheckResult** have an optional *description* parameter; you are encouraged to use it.

This shows using a few of these ways to add descriptions for a custom verb:

```cpp
auto CloseDialog(int expected_model_count) {
  auto steps = Steps(
      PressButton(kCloseDialogButtonId),
      WaitForHide(kDialogElementId),
      CheckResult([this](){ return GetModel()->count(); },
                  expected_model_count,
                  base::StringPrintf("Check model count equals %d",
                                     expected_model_count)));
  AddDescriptionPrefix(steps, "CloseDialog( %s )");
  return steps;
}
```

### Logging {#logging}

Tests will often [produce sufficient information](#test-log-output,-what-if-my-test-fails?) on failure to track down the problem, so it’s not necessary to add copious logging to your tests. However, if you want to add additional logging, [we’ve covered how here](#how-do-i-add-logging-to-my-test?).

---

## Test Log Output, What if my Test Fails? {#test-log-output,-what-if-my-test-fails?}

If a Kombucha test fails during a test step or while waiting for a test step, information will be printed into the test log output including a detailed error message and stack trace for the failure. Test timeouts are a bit more complicated, but contain the same information if you scroll down far enough. Examples are given below.

Note that if a test fails in CQ, if you click on the failed build in gerrit, at the top the failed tests will be listed; you can expand the test to see the output and call stacks. If a test fails locally, the information should be printed directly to the console.

### Step Failure Log {#step-failure-log}

On a normal test failure, a description of the failure will be displayed. If a **Check** or **EnsureNotPresent** step fails, there will be additional information before the general failure message, as you can see below.

The first thing you will notice is the specific information about the test step that failed. Following that is a dump of the entire tree of named UI elements that Kombucha is aware of. Between these two, you can hopefully determine how the actual state differs from the state you had expected.

Note that the dumped elements may include the following tags:

* \[CURRENT CONTEXT\] \- this is what the test believes to be the context for the current step \- i.e. the place where it will look for elements, or look for them first if the step is **InAnyContext()**. If there are nested control structures like **If()** and **InParallel()**, only the top-level context is reported.
* \[ACTIVE\] \- for tests involving the Views framework, this is the active Widget.
* \[FOCUS\] \- for tests involving the Views framework, this is the focused View.

Views and Widgets are also laid out in a tree that represents ownership, though it may not be direct ownership - for example, since the toolbar view does not have an identifier, toolbar buttons show up directly under TopContainerView.

```
[41980:26288:0223/135034.630:ERROR:interactive_test.cc(285)] Expected element ElementIdentifier 140696530034808 [kTabStripElementId] not to be present but it was present.
Interactive test failed on step 3 (EnsureNotPresent( kTabStripElementId, 0 ): WaitForComplete()) with reason kFailedForTesting; step type kCustomEvent; id ElementIdentifier 140696527641416 [kInteractiveTestPivotElementId]; element 000001A0C6762AF0

UI Elements
  ╰─[CURRENT CONTEXT] [ACTIVE] Tabbed browser window, 1 tab(s) (active: 0) profile Default at x:10-848 y:10-934 (838x924)
     ├─WebContents in tab 0 - ../../chrome/test/interaction/interactive_browser_test_browsertest.cc::34::kWebContentsId at x:32-826 y:116-909 (794x793) with URL "about:blank"
     ├─Pivot element (part of test automation)
     ╰─Widget "BrowserFrame" at x:10-848 y:10-934 (838x924)
        ╰─BrowserView - kBrowserViewElementId at x:32-826 y:29-909 (794x880)
           ├─TopContainerView - kTopContainerElementId at x:32-826 y:29-116 (794x87)
           │  ├─TabStripRegionView - kTabStripRegionElementId at x:32-706 y:29-70 (674x41)
           │  │  ├─TabSearchButton - kTabSearchButtonElementId at x:38-66 y:29-70 (28x41)
           │  │  ├─TabStrip - kTabStripElementId at x:60-316 y:29-70 (256x41)
           │  │  │  ╰─Tab - kTabElementId at x:60-316 y:29-70 (256x41)
           │  │  │     ╰─TabIcon - kTabIconElementId at x:77-99 y:38-60 (22x22)
           │  │  ╰─TabStripControlButton - kNewTabButtonElementId at x:310-338 y:29-70 (28x41)
           │  ├─BackForwardButton - kToolbarBackButtonElementId at x:37-71 y:75-109 (34x34)
           │  ├─BackForwardButton - kToolbarForwardButtonElementId at x:73-107 y:75-109 (34x34)
           │  ├─ReloadButton - kReloadButtonElementId at x:109-143 y:75-109 (34x34)
           │  ├─LocationIconView - kLocationIconElementId at x:157-181 y:80-104 (24x24)
           │  ├─[FOCUSED] OmniboxViewViews - kOmniboxElementId at x:189-482 y:80-104 (293x24)
           │  ├─StarView - kBookmarkStarViewElementId at x:650-674 y:80-104 (24x24)
           │  ├─PinnedToolbarActionsContainer - kPinnedToolbarActionsContainerElementId at x:695-749
           │  │  ├─PinnedActionToolbarButton - kPinnedActionToolbarButtonElementId at x:695-729 y:75-109
           │  │  ╰─View - kPinnedToolbarActionsContainerDividerElementId at x:740-742 y:84-100 (2x16)
           │  ├─AvatarToolbarButton - kToolbarAvatarButtonElementId at x:751-785 y:75-109 (34x34)
           │  ╰─BrowserAppMenuButton - kToolbarAppMenuButtonElementId at x:787-821 y:75-109 (34x34)
           ╰─ContentsWebView - ContentsWebView::kContentsWebViewElementId at x:32-826 y:116-909 (794x793

Stack trace:
Backtrace:
        ui::test::internal::InteractiveTestPrivate::OnSequenceAborted [0x00007FF66ED4E3A2+658] (o:\ui\base\interaction\interactive_test_internal.cc:134)
[[rest of stack trace omitted]]
```

#### A Note About “Step Numbers” {#a-note-about-“step-numbers”}

As you can see above, the test reports that it failed on “step 3”. However, this may not be the third verb in the test; the step itself refers to the underlying **InteractionSequence**, not the top-level test (we may improve this in the future). So the step number will typically be greater than or equal to the actual test step. *To understand what failed, look at the [step description](#describe-your-steps) and ID or name instead.*

### Timeout Test Failure Log {#timeout-test-failure-log}

The following is an example of test output where a test times out during a step. You can tell from the log that:
* The test timed out.
* The uppermost run loop was the Kombucha run loop in **InteractionSequence**; this means the test didn’t deadlock in a nested run loop but rather failed waiting for a step.
* The step that failed was waiting for a specific UI element to be shown.

From this you would deduce that the element the test was waiting for never appeared or the state it was waiting for never occurred.

```
RunLoop::Run() timed out. Timeout set at ProxyRunTestOnMainThreadLoop@content\public\test\browser_test_base.cc:823.
Interactive test failed on step 2 (WaitForShow()) with reason kSequenceDestroyed; step type kShown; id ElementIdentifier 140696184266576 [kAutofillCreditCardSuggestionEntryElementId]

UI Elements
[[OMITTED FOR BREVITY]]

Stack trace:
Backtrace:
        base::test::ScopedRunLoopTimeout::ScopedRunLoopTimeout::<lambda_0>::operator() [0x00007FF658FEA5F9+169] (o:\base\test\scoped_run_loop_timeout.cc:54)
        base::internal::FunctorTraits<`lambda at ../../base/test/scoped_run_loop_timeout.cc:51:9',void>::Invoke<const `lambda at ../../base/test/scoped_run_loop_timeout.cc:51:9' &,const base::Location &,const base::RepeatingCallback<std::Cr::basic_string<char,std [0x00007FF658FEA4B3+99] (o:\base\functional\bind_internal.h:639)
        base::internal::InvokeHelper<0,void,0,1>::MakeItSo<const `lambda at ../../base/test/scoped_run_loop_timeout.cc:51:9' &,const std::Cr::tuple<base::Location,base::RepeatingCallback<std::Cr::basic_string<char,std::Cr::char_traits<char>,std::Cr::allocator<cha [0x00007FF658FEA43D+93] (o:\base\functional\bind_internal.h:943)
        base::internal::Invoker<base::internal::BindState<`lambda at ../../base/test/scoped_run_loop_timeout.cc:51:9',base::Location,base::RepeatingCallback<std::Cr::basic_string<char,std::Cr::char_traits<char>,std::Cr::allocator<char> > ()> >,void (const base::L [0x00007FF658FEA3CC+44] (o:\base\functional\bind_internal.h:1038)
        base::internal::Invoker<base::internal::BindState<`lambda at ../../base/test/scoped_run_loop_timeout.cc:51:9',base::Location,base::RepeatingCallback<std::Cr::basic_string<char,std::Cr::char_traits<char>,std::Cr::allocator<char> > ()> >,void (const base::L [0x00007FF658FEA2F8+72] (o:\base\functional\bind_internal.h:1002)
        base::OnceCallback<void (const base::Location &)>::Run [0x00007FFB89D3A14B+139] (o:\base\functional\callback.h:153)
        base::`anonymous namespace'::OnRunLoopTimeout [0x00007FFB89D372D1+49] (o:\base\run_loop.cc:47)
        base::internal::FunctorTraits<void (*)(base::RunLoop *, const base::Location &, base::OnceCallback<void (const base::Location &)>),void>::Invoke<void (*)(base::RunLoop *, const base::Location &, base::OnceCallback<void (const base::Location &)>),base::Run [0x00007FFB89D3B14B+107] (o:\base\functional\bind_internal.h:654)
        base::internal::InvokeHelper<0,void,0,1,2>::MakeItSo<void (*)(base::RunLoop *, const base::Location &, base::OnceCallback<void (const base::Location &)>),std::Cr::tuple<base::internal::UnretainedWrapper<base::RunLoop,base::unretained_traits::MayNotDangle, [0x00007FFB89D3B0CB+123] (o:\base\functional\bind_internal.h:943)
        base::internal::Invoker<base::internal::BindState<void (*)(base::RunLoop *, const base::Location &, base::OnceCallback<void (const base::Location &)>),base::internal::UnretainedWrapper<base::RunLoop,base::unretained_traits::MayNotDangle,0>,base::Location, [0x00007FFB89D3B042+34] (o:\base\functional\bind_internal.h:1038)
        base::internal::Invoker<base::internal::BindState<void (*)(base::RunLoop *, const base::Location &, base::OnceCallback<void (const base::Location &)>),base::internal::UnretainedWrapper<base::RunLoop,base::unretained_traits::MayNotDangle,0>,base::Location, [0x00007FFB89D3AF7E+62] (o:\base\functional\bind_internal.h:989)
        base::OnceCallback<void ()>::Run [0x00007FFB89BC2FA7+119] (o:\base\functional\callback.h:153)
        base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> >::ForwardOnce<> [0x00007FFB89D3B846+38] (o:\base\cancelable_callback.h:128)
        base::internal::FunctorTraits<void (base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> >::*)(),void>::Invoke<void (base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> >::*)(),base::WeakPtr<base::internal::CancelableCallbackI [0x00007FFB89D3BB4F+31] (o:\base\functional\bind_internal.h:764)
        base::internal::InvokeHelper<1,void,0>::MakeItSo<void (base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> >::*)(),std::Cr::tuple<base::WeakPtr<base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> > > > > [0x00007FFB89D3BACF+79] (o:\base\functional\bind_internal.h:970)
        base::internal::Invoker<base::internal::BindState<void (base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> >::*)(),base::WeakPtr<base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> > > >,void ()>::RunImpl<void (base::interna [0x00007FFB89D3BA72+34] (o:\base\functional\bind_internal.h:1038)
        base::internal::Invoker<base::internal::BindState<void (base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> >::*)(),base::WeakPtr<base::internal::CancelableCallbackImpl<base::OnceCallback<void ()> > > >,void ()>::RunOnce [0x00007FFB89D3B9DE+62] (o:\base\functional\bind_internal.h:989)
        base::OnceCallback<void ()>::Run [0x00007FFB89BC2FA7+119] (o:\base\functional\callback.h:153)
        base::TaskAnnotator::RunTaskImpl [0x00007FFB89DDDF82+482] (o:\base\task\common\task_annotator.cc:180)
        base::TaskAnnotator::RunTask<`lambda at ../../base/task/sequence_manager/thread_controller_with_message_pump_impl.cc:492:11'> [0x00007FFB89E4ABB3+147] (o:\base\task\common\task_annotator.h:89)
        base::sequence_manager::internal::ThreadControllerWithMessagePumpImpl::DoWorkImpl [0x00007FFB89E4A6D5+3045] (o:\base\task\sequence_manager\thread_controller_with_message_pump_impl.cc:498)
        base::sequence_manager::internal::ThreadControllerWithMessagePumpImpl::DoWork [0x00007FFB89E495CB+363] (o:\base\task\sequence_manager\thread_controller_with_message_pump_impl.cc:341)
        base::MessagePumpForUI::DoRunLoop [0x00007FFB89F8A53E+190] (o:\base\message_loop\message_pump_win.cc:212)
        base::MessagePumpWin::Run [0x00007FFB89F89210+192] (o:\base\message_loop\message_pump_win.cc:79)
        base::sequence_manager::internal::ThreadControllerWithMessagePumpImpl::Run [0x00007FFB89E4B433+691] (o:\base\task\sequence_manager\thread_controller_with_message_pump_impl.cc:652)
        base::RunLoop::Run [0x00007FFB89D369CD+845] (o:\base\run_loop.cc:140)
        ui::InteractionSequence::RunSynchronouslyForTesting [0x00007FFB74796C86+166] (o:\ui\base\interaction\interaction_sequence.cc:460)
        ui::test::InteractiveTestApi::RunTestSequenceImpl [0x00007FF65A393F89+345] (o:\ui\base\interaction\interactive_test.cc:378)
[[rest of stack trace omitted]]
```

## Restarting the Browser {#restarting-the-browser}

Some tests require a browser to shut down and start back up, for example, to ensure that a setting or some user data was saved properly. The easiest way to do this is by using a **PRE\_** test (note: not yet available on Android):

```cpp
IN_PROC_BROWSER_TEST_F(MyTestFixture, PRE_MyRestartTest) {
  RunTestSequence(
    // Set up browser state and run test actions before the restart here
  );
}

IN_PROC_BROWSER_TEST_F(MyTestFixture, MyRestartTest) {
  RunTestSequence(
    // Set up browser state and run test actions after the restart here
  );
}
```

It is also possible to simulate a restart in several different ways, though it’s not yet clear if these are reliable in the context of a Kombucha test. One such mechanism is used in [SessionRestoreInteractiveUitest::QuitBrowserAndRestore()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/sessions/session_restore_interactive_uitest.cc;l=45). So if you had access to this method or one modeled after it in your Kombucha test, you might do:

```cpp
RunTestSequence(
  // Do pre-restart logic here.
);

Browser* const new_browser = QuitBrowserAndRestore(browser());

RunTestSequenceInContext(
  BrowserElements::From(new_browser)->GetContext(),
  // Do post-restart logic here.
);
```

## Sample Code {#sample-code}

* [CodeSearch query that returns most existing Kombucha tests](https://source.chromium.org/search?q=RunTestSequence%20filepath:chrome%2Fbrowser&sq=&ss=chromium).
* [Deck for onboarding to Kombucha test writing.](https://go/kombucha-intro)

# FAQ {#faq}

## Writing Tests with Kombucha {#writing-tests-with-kombucha}

### How do I use Kombucha in my tests? {#how-do-i-use-kombucha-in-my-tests?}

Create a new test class by inheriting from **InteractiveBrowserTest**. Then use **IN\_PROC\_BROWSER\_TEST\_F** or **IN\_PROC\_BROWSER\_TEST\_P** as you would with **InProcessBrowserTest**.

In your test, after any test-specific setup, call **RunTestSequence()** and list your test steps.

```cpp
class MyInteractiveTest : public InteractiveBrowserTest {
 public:
  MyInteractiveTest() = default;
  ~MyInteractiveTest() override = default;

  // Add any additional stuff you need here. Don’t forget to call the base
  // class versions of SetUp(), SetUpOnMainThread(), and
  // TearDownOnMainThread() if you override those methods.
};

IN_PROC_BROWSER_TEST_F(MyInteractiveTest, Test1) {
  // Additional test setup goes here.
  RunTestSequence(/* Test steps go here... */);
}
```

### What should go in **SetUp\[OnMainThread\]** or at the top of my test vs. in **RunTestSequence**? {#what-should-go-in-setup[onmainthread]-or-at-the-top-of-my-test-vs.-in-runtestsequence?}

It’s okay to put common setup in **SetUpOnMainThread()**, or to put a bit of setup at the top of an individual test (you may sometimes need test-specific declarations, for example).

However, if the setup can be expressed as Kombucha test steps, it’s probably better to create a custom verb that you can include at the top of each test sequence.

```cpp
class MyInteractiveTest : public InteractiveBrowserTest {
 public:
  MyInteractiveTest() = default;
  ~MyInteractiveTest() override = default;

  auto DoCommonSetup() {
    auto steps = Steps(
      // Common set-up steps go here.
      // If any of the setup steps fail, the test sequence will fail.
    );
    AddDescriptionPrefix(steps, "DoCommonSetup()");
    return steps;
  }
};

IN_PROC_BROWSER_TEST_F(MyInteractiveTest, Test1) {
  RunTestSequence(
    DoCommonSetup(),
    // Rest of test goes here.
  );
}
```

### How should I test my feature using Kombucha? {#how-should-i-test-my-feature-using-kombucha?}

Ideally, create a different test for each user journey, from beginning to end. This provides the best regression test coverage. For new features, your UX spec or PRD should have a list of user journeys supported by your feature.

### What if I already have a class that inherits from **Test** or **InProcessBrowserTest**? {#what-if-i-already-have-a-class-that-inherits-from-test-or-inprocessbrowsertest?}

First, if the test inherits directly from **Test**, **ViewsTestBase**, or **InProcessBrowserTest**, you can upgrade it directly to the equivalent Kombucha test fixture:

* **Test** → **InteractiveTest**
* **ViewsTestBase** → **InteractiveViewsTest**
* **InProcessBrowserTest** → **InteractiveBrowserTest**

However, if for some reason this is not possible, you can instead inherit from **InteractiveViewsTestApi** or **InteractiveBrowserTestApi**. There is some additional boilerplate you will have to add to your test’s set-up and tear-down. You can look at the implementation of **InteractiveViewsTest** and **InteractiveBrowserTest** to see what you will need to add to your own test.

### Can I use Kombucha in **browser\_tests** or just **interactive\_ui\_tests**? {#can-i-use-kombucha-in-browser_tests-or-just-interactive_ui_tests?}

You can create tests using **InteractiveBrowserTest** in both **browser\_tests** and **interactive\_ui\_tests**. However, you should avoid the following action types in **browser\_tests**  as they require stronger process isolation:

* Window activation
* Mouse input (move, click, drag)

### Where should I put my tests? {#where-should-i-put-my-tests?}

Tests should live next to the implementation code, just as existing interactive and browser tests do. Browser tests should end with **\_browsertest.cc** and interactive ui tests should end with **\_uitest.cc**, as normal. Your test fixture class that derives from one of the “**Interactive…Tests**” classes can go in an existing test file or a new file.

Note you can use a Kombucha test fixture to run normal browser tests; **InteractiveBrowserTest** is backwards compatible with **InProcessBrowserTest**.

### Can I use Kombucha without a browser/browser window? {#can-i-use-kombucha-without-a-browser/browser-window?}

You can derive from **InteractiveViewsTest** if you have access to Views. However you will need to set up your own widget and call **SetContextWidget()** as appropriate.

### Can I write Kombucha tests for Ash or a ChromeOS app? {#can-i-write-kombucha-tests-for-ash-or-a-chromeos-app?}

Currently, you can use **InteractiveViewsTest** but there is no, for example, “InteractiveAshTest”. We should probably make one\!

## Elements, Activation, and Events {#elements,-activation,-and-events}

### What is an “element”? {#what-is-an-“element”?}

An *element* is any bit of UI that can be referenced in a Kombucha test. For Views, this is any view which is (a) visible and (b) has an **ElementIdentifier** assigned to it (via the **kElementIdentifierKey** property).

There are other things which count as elements:

* menu items (including native Mac items)
* Instrumented WebContents
* help bubble anchors in WebUI documents

Almost every Kombucha verb references a target element. Not every verb can be applied to every kind of element.

### What is a “context”? {#what-is-a-“context”?}

A *context* represents a single window or process that contains one or more elements. Common contexts are:

* A browser window (plus its menus, most of its dialogs, instrumented WebContents, etc.)
* A PWA window (again, plus menus, etc.)
* A dialog not tied directly to any browser window, such as a tab-modal dialog
* A WebUI that contains help bubble anchors (note however that WebContents instrumented during tests share their context with their host browser)

The default context for a Kombucha test is the initial browser window that’s created by the test; all verbs will by default search for elements in that context. You can use **InContext**, **InAnyContext**, or **InSameContext** to change that default behavior.

For example:

```cpp
IN_PROC_BROWSER_TEST_F(MyInteractiveTest, Test1) {
  RunTestSequence(
    InAnyContext(PressButton(kAppMenuButtonElementId)),
    InSameContext(SelectMenuItem(kMoreToolsMenuItemElementId)),
    // ...
  );
}
```

### What does it mean for an element to be “shown” or “hidden”? {#what-does-it-mean-for-an-element-to-be-“shown”-or-“hidden”?}

A View is shown if it is visible and attached to a widget. An instrumented WebContents is shown when its webview is visible and its page is fully loaded. In general, if an element is visible to the user in its complete form, it is shown, and if it’s hidden or not ready in some way, it’s hidden.

### What does it mean for an element to be “activated”? {#what-does-it-mean-for-an-element-to-be-“activated”?}

This is dependent on the implementation, but for Views, pressing a button causes it to emit an “activated” event. Tabs are activated when they are clicked on, etc. In general, activation means “user does a default action on this element”.

### What are custom events for/why should I use them? {#what-are-custom-events-for/why-should-i-use-them?}

A custom event is an event like activation that is sent through a UI element. Your test can wait for a custom event in order to proceed.

Custom events [can replace runloops and polling loops in your code](#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?). If you set up your system to emit a custom event when an asynchronous or time-consuming operation completes, your test can then wait for the event.

```cpp
// This is acceptable but not ideal:
IN_PROC_BROWSER_TEST_F(MyInteractiveTest, TestWithRunLoop) {
  RunTestSequence(
    WithView(
      kDoProcessingTaskElementId,
      [](DoTaskButton* do_task){
        base::RunLoop run_loop(base::RunLoop::Type::...);
        do_task->controller_for_testing()->SetOnTaskCompleteForTesting(
            run_loop.QuitClosure());
        InteractionTestUtilViews::PressButton(do_task);
        run_loop.Run();
      }),
    // Continue test ...
  );
}

// This is better and more concise.
// In this case, the controller object knows to send the event when the
// task completes. It does not need to know that the test is running, and
// also is no longer necessary to expose its functionality in a “for
// testing” API:
IN_PROC_BROWSER_TEST_F(MyInteractiveTest, TestWithEvent) {
  RunTestSequence(
    PressButton(kDoProcessingTaskElementId),
    WaitForEvent(
        kDoProcessingTaskElementId,
        kDoProcessingTaskCompleteEvent),
    // Continue test ...
  );
}
```

### Can I send a custom event if I don’t have access to Views from my code? {#can-i-send-a-custom-event-if-i-don’t-have-access-to-views-from-my-code?}

Yes. You can use something like the following:

```cpp
auto* const el =
    ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        id, context);
ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(el, event_type);
```

For id you can use any common id, like **kBrowserViewElementId**. If you don’t care about context, you can use **GetElementInAnyContext()** instead.

### Can I send a custom event if I don’t have a specific element to send it through? {#can-i-send-a-custom-event-if-i-don’t-have-a-specific-element-to-send-it-through?}

Yes. **kBrowserViewElementId** is always available in browser-based tests, so you can use that. You can also create a local [TestElement](https://source.chromium.org/chromium/chromium/src/+/main:ui/base/interaction/element_test_util.h;l=43?q=TestElement&sq=&ss=chromium) if you prefer, or don’t have access to a browser window for the test. In fact, that’s how Interactive\*Test sends messages to itself\!

### How do I wait for a dialog or menu to open? {#how-do-i-wait-for-a-dialog-or-menu-to-open?}

Identify an element in the dialog or menu with a known **ElementIdentifier**, then do a **WaitForShow**:

```cpp
RunTestSequence(
   // Do thing that shows dialog or menu.
   WaitForShow(kMyKnownDialogOrMenuElementId),
   // Perform rest of test.
);
```

### How do I wait for a dialog or menu to close? {#how-do-i-wait-for-a-dialog-or-menu-to-close?}

Just as you can use **WaitForShow** to determine when a dialog appears, you can use **WaitForHide** to determine when it closes:

```cpp
RunTestSequence(
   // Do thing that hides dialog or menu.
   WaitForHide(kMyKnownDialogOrMenuElementId),
   // Continue with the rest of the test.
);
```

### How do I make sure something *isn’t* present in the UI? {#how-do-i-make-sure-something-isn’t-present-in-the-ui?}

We provide **EnsureNotPresent** to check that something *isn’t* visible:

```cpp
RunTestSequence(
   // Do thing that should not show the item.
   EnsureNotPresent(kElementThatShouldNotBeVisibleId),
   // Continue with the rest of the test.
);
```

### How do I know what function signature a verb expects for callbacks? {#how-do-i-know-what-function-signature-a-verb-expects-for-callbacks?}

Many verbs, from **Do** to **AfterEvent** to **CheckView** expect you to pass in some code to be run during step execution. [This can usually be anything callable](#callbacks), from a lambda to a function pointer to a **OnceCallback** or **RepeatingCallback**.

You can determine the signature of the function by looking at the declaration of the verb. For example, CheckView is declared as follows (some unimportant bits have been elided):

```cpp
template <...
    typename = ui::test::internal::RequireSignature<F, bool(V*)>>
static StepBuilder CheckView(ElementSpecifier view, F&& check);
```

That last template parameter is the important one\! It tells you that this version of **CheckView** expects a check function with a signature bool(V\*), where V is a View class. *This will be enforced at compile time*, hopefully with a useful error message if you screw up\!

Now, let’s take a look at **AfterEvent**:

```cpp
template <...
    typename =
        internal::RequireCompatibleSignature<
            T,
            void(InteractionSequence*, TrackedElement*)>>
static StepBuilder AfterEvent(
    ElementSpecifier element,
    CustomElementEventType event_type,
    T&& step_callback);
```

This declaration tells you that step\_callback needs to be *compatible* with the signature void(InteractionSequence\*, TrackedElement\*). Compatibility means that the callback you pass in may omit one or both arguments, starting from the left. So any of the following signatures are valid:

* void()
* void(TrackedElement\*)
* void(InteractionSequence\*, TrackedElement\*)

Again, this is enforced at compile time; if you screw up, the compiler will let you know.

### When passing a callback or check to a verb, do I need to call Bind? {#when-passing-a-callback-or-check-to-a-verb,-do-i-need-to-call-bind?}

In general, no. See [the section on callbacks under Best Practices](#callbacks).

## Performing Actions {#performing-actions}

### How do I click a button? {#how-do-i-click-a-button?}

You can use the **PressButton** action verb:

```cpp
RunTestSequence(
   PressButton(kButtonElementId),
);
```

By default, **PressButton** simulates sending an event to the button. What event type is used by default depends on the button and OS, but you can specify keyboard, mouse, or touch explicitly by using an optional second argument.

### How do I input text? {#how-do-i-input-text?}

Use the **EnterText** action verb:

```cpp
RunTestSequence(
  EnterText(kMyTextFieldId, new_text_content),
);
```

By default, the text replaces the existing text, but there are other modes (which you can pass as a third, optional parameter).

### Can I send keypresses as part of a test? {#can-i-send-keypresses-as-part-of-a-test?}

You can use **SendAccelerator** to send an accelerator to a target element. You will still need to create or look up the accelerator for the command you want on the current platform.

```cpp
const Accelerator accelerator;
GetAcceleratorProvider()->GetAcceleratorForCommandId(
    IDC_MY_COMMAND, &accelerator);
RunTestSequence(
  SendAccelerator(kMyControlElementId, accelerator),
);
```

Note that this is designed to send accelerators and control keys specifically, and is not designed for arbitrary text input. Use **EnterText** for that instead.

### How do I select an item from a menu or dropdown? {#how-do-i-select-an-item-from-a-menu-or-dropdown?}

Use **SelectDropdownItem**. This selects the *N*th item from the dropdown:

```cpp
RunTestSequence(
  SelectDropdownItem(kMyDropdownElementId, 2),
);
```

### How do I move or click the mouse? {#how-do-i-move-or-click-the-mouse?}

Use **MoveMouseTo**, **ClickMouse**, and **ReleaseMouse** to send input. You must be in **interactive\_ui\_tests** or a similar exclusive, single-process test for mouse input to work reliably:

```cpp
RunTestSequence(
   MoveMouseTo(kButtonElementId),
   ClickMouse()
);
```

**ClickMouse** defaults to left mouse button and performs a press-and-release, but you can change these behaviors with optional arguments.

### Should I use **PressButton** or **MoveMouse**/**ClickMouse**? {#should-i-use-pressbutton-or-movemouse/clickmouse?}

It depends\! Mouse input is somewhat less reliable, so if you don’t need to explicitly simulate it, then **PressButton** is a better choice. However, if you want to make sure that no UI is obscuring the element you want to click, or need to see how the UI responds to hovering and clicking an element explicitly, then mouse verbs are better. Note that you can only use mouse verbs in **interactive\_ui\_tests** or similar single-process tests, as otherwise, other processes might try to move the mouse as well.

Also note that depending on the platform, even the mouse verbs may only be simulations and may not actually move the system mouse caret; different operating systems allow us to seize control of input devices in different ways (or not at all). All of our verbs are always best-effort.

### How do I do stuff that isn’t covered by an action like **PressButton()**? {#how-do-i-do-stuff-that-isn’t-covered-by-an-action-like-pressbutton()?}

Check out verbs like **SelectMenuItem**, **DoDefaultAction**, or **Confirm**, which can send “accept” type inputs to elements that are not buttons. For example, the default action for a tab is to select a tab; the confirmation action for the omnibox is to navigate to whatever URL you typed in.

### What if I have a common step or set of steps I want to do in multiple tests? {#what-if-i-have-a-common-step-or-set-of-steps-i-want-to-do-in-multiple-tests?}

This is an excellent chance to use a *custom verb*. A custom verb is a function in your test fixture that returns a **StepBuilder** or **MultiStep**; you can call this function to inject the custom verb into your test sequence the same way you would any other verb. By convention, custom verbs return **auto**.

Here’s an example:

```cpp
class MyFeaturePage2Test : public InteractiveBrowserTest {
 public:
  // ...
  auto DoCommonSetUp() {
    auto steps = Steps(
      PressButton(kMyFeatureEntryPointElementId),
      WaitForShow(kMyFeatureUIElementId),
      SelectDropdownItem(kMyFeatureModeElementId, 2),
      WaitForShow(kMyFeaturePage2ElementId));
    AddDescriptionPrefix(steps, "DoCommonSetUp()");
    return steps;
  }
};

IN_PROC_BROWSER_TEST_F(MyFeaturePage2Test, Test1) {
  RunTestSequence(
    DoCommonSetUp(),
    // Do rest of test 1 here.
  );
}

IN_PROC_BROWSER_TEST_F(MyFeaturePage2Test, Test2) {
  RunTestSequence(
    DoCommonSetUp(),
    // Do rest of test 2 here.
  );
}
```

Note how calling “DoCommonSetUp()”, the common steps are injected into the test sequence.

## Test Checks {#test-checks}

### How do I perform checks in my test? {#how-do-i-perform-checks-in-my-test?}

There are a number of **Check…** verbs, most of which allow you to specify some kind of test or call, and sometimes a **Matcher** which can be used to check the return value (if not specified, the matcher is simply “true”).

```cpp
RunTestSequence(
   // Check that the preference being tested has the correct value.
   CheckResult(&GetPrefValue, “option1”),
);
```

There’s also **CheckElement**, **CheckView**, and **CheckViewProperty** that provide more specific tests. The **CheckView…** verbs will automatically cast the element to the correct view type, or fail if it is not. Examples:

```cpp
RunTestSequence(
   CheckViewProperty(
       kMyTextFieldElementId, &TextField::GetText, u“expected text”)
);
```

### Can I still use **EXPECT…()** and **ASSERT…()**? {#can-i-still-use-expect…()-and-assert…()?}

Yes, you can use **EXPECT\_…** and **ASSERT\_…** in callbacks like those you can specify for Do, **AfterShow**, **WithView**, etc. However, it’s usually considered to be better to use one of the **Check…** verbs if you can, because they automatically provide both information about the mismatched value or unexpected condition *and* the step the test failed on, whereas **EXPECT** and **ASSERT** only provide call stacks.

A comparison:

```cpp
// This:
CheckViewProperty(
    kMyTextFieldElementId, &TextField::GetText, u“expected text”)

// Is better than this:
WithView(
    kMyTextFieldElementId,
    [](TextField* textfield) {
      EXPECT_EQ(textfield->GetText(), u“expected text”);
    });
```

### Do I need to pass a testing::Matcher when I use a Check? {#do-i-need-to-pass-a-testing::matcher-when-i-use-a-check?}

Not necessarily.

If you are just checking that some condition is true, there are versions of most **Check** verbs that take a callback that returns a boolean, and don’t require a matcher.

If you are matching against a specific value, you do not need to specify **testing::Eq**, you can just pass the value. You can also pass a literal string (of the appropriate character width) to match e.g. a **std::string** or **std::u16string**.

The following should all be equivalent:

```cpp
  // If your test function should return true, no matcher is required.
  Check([](){ return !ErrorOccurred(); }),

  // Match the result against an exact value.
  CheckResult(&ErrorOccurred, false),

  // Use an equality matcher.
  CheckResult(&ErrorOccurred, testing::Eq(false)),

  // Use some other arbitrary matcher.
  CheckResult(&ErrorOccurred, testing::Ne(true))
```

### How do I do a pixel (Skia Gold) test? {#how-do-i-do-a-pixel-(skia-gold)-test?}

Taking screenshots requires a few steps:

1. Add a **Screenshot** step to your test.
2. Add your test name to [pixel\_tests.filter](https://source.chromium.org/chromium/chromium/src/+/main:testing/buildbot/filters/pixel_tests.filter?q=pixel_tests.filter&ss=chromium).
3. Figure out what to do if your test runs in a test fixture that does not support pixel tests.

Step (3) is critically important to avoid test failures. By default, trying to perform a test step that isn’t supported in the current test fixture results in a test failure. However, you can call **SetOnIncompatibleAction** before your **Screenshot** test to select the desired behavior. See the [OnIncompatibleAction enum](https://source.chromium.org/chromium/chromium/src/+/main:ui/base/interaction/interactive_test_internal.h;l=48) for options. In general:

* Use **kSkipTest** if you only want your test to run in pixel test-compatible executables \- that is, if the primary reason for having your test is to take a screenshot.
* Use **kIgnoreAndContinue** if you’re taking a random screenshot in the middle of a test that you want to have run on all platforms \- that is, if you’re writing a regression test and only incidentally want to check a screenshot if it’s an option.

## Unnamed and Duplicate UI Elements {#unnamed-and-duplicate-ui-elements}

### I need to check an element that doesn’t have an identifier. How do I do it? {#i-need-to-check-an-element-that-doesn’t-have-an-identifier.-how-do-i-do-it?}

You have two options:

* Do a **Check** on a known element from which you can access the element in question.
* *Name* the element, then use that name in the rest of the test sequence.

*Naming* is a way to assign a unique name that is local to one specific test and can be specified at runtime. **NameView**, **NameViewRelative**, **NameChildView**, and **NameDescendantView** are all ways to name views. Here’s an example:

```cpp
constexpr char kThirdTabName[] = “third tab”;
RunTestSequence(
   // Note: these are 0-indexed.
   NameDescendantViewByType<Tab>(kTabStripElementId, kThirdTabName, 2),
   CheckViewProperty(kThirdTabName, &Tab::IsActive));
```

### I need to refer to a specific element, but its identifier is not unique. {#i-need-to-refer-to-a-specific-element,-but-its-identifier-is-not-unique.}

If the element is unique within a specific browser window, you can use the context of that window to uniquely identify that element:

```cpp
RunTestSequence(
  InContext(other_browser_window_context,
            PressButton(kAppMenuButtonElementId)),
  InSameContext( /* Do some more stuff here */ )
);
```

If there is more than one element with the same identifier in the same context, you must decide if you care which one you get. If you don’t care, then simply refer to the element by identifier; you’re guaranteed to get *one* of them:

```cpp
RunTestSequence(
  // This will grab any currently visible tab group header.
  WithElement(kTabGroupHeaderElementId, base::BindOnce(...)),
);
```

If you want to capture an element that is going to appear or be added to the window, but there are already elements with the same ID, you can specify that the step must refer to a discrete event \- i.e. that it can only proceed if an element *becomes newly visible* rather than just being visible when the step starts:

```cpp
RunTestSequence(
  // This will apply to the next tab group header to be created.
  AfterShow(kTabGroupHeaderElementId, base::BindOnce(...))
    .SetTransitionOnlyOnEvent(true),
);
```

Note that **SetTransitionOnlyOnEvent** can be applied to **WaitForHide**/**AfterHide** steps as well. The syntax is a bit awkward, coming after the step rather than wrapping it like other modifiers, but we may add syntactic sugar to handle it later.

If you need to remember this element for later, you can name it in the callback:

```cpp
RunTestSequence(
  // This will apply to the next tab group header to be created.
  AfterShow(
    kTabGroupHeaderElementId,
    [](InteractionSequence* seq, TrackedElement* el){
      seq->NameElement(el, kNewTabGroupHeaderName);
    })
    .SetTransitionOnlyOnEvent(true),
);
```

### How do I refer to an element in a different window? {#how-do-i-refer-to-an-element-in-a-different-window?}

Each browser or PWA window has its own *context*. A context is shared by:

* The window itself and all of its UI elements.
* Any menus attached to the window.
* Any dialogs attached to the window (but see below).

The following may have a different context from the browser window they are associated with:

* Tab-modal dialogs (since they may be moved between browser windows)
* WebUI help bubble anchors (since a tab may be moved between windows)

If the entire sequence should default to using a specific context that is not the default window, then you can use **RunTestSequenceInContext** instead of **RunTestSequence**.

If you want your test to find an element that could be anywhere, you can use **InAnyContext**, and then, if you need to keep referring to that specific window, use **InSameContext**:

```cpp
RunTestSequence(
  InAnyContext(WaitForShow(kMyTabModalDialogElementId)),
  InSameContext(
    // Do more stuff in the tab-modal dialog...
  ),
  // The rest of the test goes here, defaulting to the main window.
);
```

If you know the context or window you want to use, you can specify it with **InContext**:

```cpp
RunTestSequence(
  InContext(
    BrowserElements::From(incognito_browser)->GetContext(),
    PressButton(kAppMenuButtonElementId)),
    // Do more stuff in this context here…
  ),
  // Rest of the test goes here, defaulting to the main window.
);
```

There are cases where you may not know the context for one or more steps ahead of time; see [this section](#testing-element\(s\)-in-another-context) if you run into issues around elements in other windows or tab modal dialogs.

## Troubleshooting {#troubleshooting}

### What are the ways a Kombucha test can fail? {#what-are-the-ways-a-kombucha-test-can-fail?}

A Kombucha test can fail in the following ways:

* A **Check** verb fails to match the required condition.
* A verb that requires a condition to be true at start, such as **EnsureNotPresent**, **WithElement**, etc. does not meet its precondition.
* A verb expects an element of a specific type, but the actual element is the wrong type.
  * For example, if the element passed to **WithView** is not a view or is not a view of the correct type.
* A callback contains an **EXPECT** or **ASSERT** statement that fails.
* A verb expects an element to eventually be present or a condition to eventually become true, but it never does and the test times out.
  * This includes things like **PressButton** that don’t require the element to be immediately present unless you explicitly add **SetMustBeVisibleAtStart(true)**.
* A verb that is not supported on the current platform or configuration (such as **Screenshot** or **ActivateSurface**)  is executed, and you have not preceded it by changing the **SetOnIncompatibleAction** behavior.

### My test failed; how do I figure out why? {#my-test-failed;-how-do-i-figure-out-why?}

The first thing to do is [look at the logging output](#test-log-output,-what-if-my-test-fails?). Here is what you should see depending on the failure type:

| Failure Type | What You Will See |
| :---- | :---- |
| Check or precondition fails, or unsupported action is attempted | Description of the step that failed and why, followed by a dump of all known elements, followed by a stack trace. |
| Timeout during test | Same as above, but with a “timed out” error message. |
| Step failure | Same as above, but the error will be something like “element hidden during step”. |
| Failure, crash, or timeout during test startup or shutdown | Error plus call stack. Look for a call stack that does not include your test function or **RunTestSequence**. |
| **EXPECT**, **ASSERT**, or crash during test execution | Error description plus call stack. No step description is given, but the call stack should indicate where the failure occurred. |

As you can see, there is a strong advantage to using **Check** verbs as they will provide a detailed description of the failure rather than just a call stack.

### The step number in the failure message doesn’t seem right? {#the-step-number-in-the-failure-message-doesn’t-seem-right?}

The step number listed is the step in the underlying **InteractionSequence** (we may change this in the future), not the count of top-level test actions. Many test actions and especially custom actions you create will have more than one step. Therefore, the “step number” is probably higher than the actual index of the top-level action in your test. Remember to [add description to your test steps](#describe-your-steps) to help differentiate them\!

### My test timed out; how do I figure out where it failed? {#my-test-timed-out;-how-do-i-figure-out-where-it-failed?}

If the timeout happened during the test sequence, the timeout message will be followed by a description of the step that failed, and then a dump of all known elements in the system, which can help you track down if an element you expected to be present is missing or misplaced.

If you do *not* see the current step failure message, that is an indication that the test may have failed during setup or tear-down, or that some kind of deadlock occurred, in which case the call fstack may provide additional information.

See [this section](#test-log-output,-what-if-my-test-fails?) for more information on how to interpret test output and call stacks.

### My test failed due to an element losing visibility; what do I do? {#my-test-failed-due-to-an-element-losing-visibility;-what-do-i-do?}

You may get an error message that looks something like the following:

```
Interactive test failed on step 2 (InAnyContext( WaitForShow() )) with reason kElementHiddenDuringStep; step type kShown; id ElementIdentifier ...
```

The key here is **kElementHiddenDuringStep**, which indicates that a test step or substep expected the element to remain visible until the next step was triggered, but it did not. This is usually due to the dialog, browser, or entire test shutting down (sometimes due to a timeout) and closing a window.

Another way this can happen is that the element you are waiting for becomes visible, and then is hidden right away, before the step can properly complete. This is often due to one of the following:

* Bad layout logic is causing the element’s visibility to oscillate rapidly. Check the call stack to get an idea why the element is disappearing.
* You’re referencing an element in a surface such as a menu or bubble that goes away when it loses focus, and it has lost focus (you’ll see this in the call stack). This can be somewhat difficult to diagnose, but a good way to minimize the chance of this happening spuriously is to make sure your test is in **interactive\_ui\_tests** and not **browser\_tests** (the latter of which is not guaranteed to be the only process on the machine, and can therefore cause tests to lose focus).
* You’re observing and waiting for a state you expected to remain stable, but its value is changing rapidly.

If the underlying system is volatile and you can’t fix it, you can sometimes work around this by using [**WithoutDelay**](#verifying-transient-objects-and-states:-withoutdelay), but this is strongly discouraged; if you have to do this temporarily, ensure that there is a “// TODO(https://crbug.com/…)” link explaining why the test needed to work around the underlying system and capturing that it eventually needs to be fixed.

If you are observing a transient state \- say you expect a value to tick to 1 but then it might continue to tick to 2, instead of writing **WaitForState(kMyState, 1\)**, you can use a matcher: **WaitForState(kMyState, testing::Ge(1))**. This is a way to avoid many problems with checking transient states. You can also just define the state differently, or use a custom event, or some other approach that does not expose the transient quality of the state. Again, in this case, **WithoutDelay** is a last resort and should be documented.

### Something changed/went away between the trigger and step callback\! {#something-changed/went-away-between-the-trigger-and-step-callback!}

This is the most common cause of use-after-free (UAF) in tests, and happens because *Kombucha is fundamentally asynchronous*. A step’s callback or action or check happens on a clean call stack after the step’s trigger, and after any events that are already pending. This prevents *most* race conditions and order-of-operation bugs in tests, but can occasionally introduce new ones.

For example, consider the case where closing a dialog destroys its controller object, but we want to verify the controller after the dialog is closed. Interact-observe-verify says wait for the dialog to close and then check the value, but by the time the check happens, the dialog is already closed and the controller is gone\!

In this case, you can use a **WithoutDelay** modifier to ensure that the checks are all done as soon as possible \- ideally on the same call stack. See [this section](#verifying-transient-objects-and-states:-withoutdelay) for more info,

### My test is still crashing, what can I do? {#my-test-is-still-crashing,-what-can-i-do?}

Look at the crash. Is it a re-entrancy issue? A deadlock? A use-after-free? A **\[D\]CHECK**?

The first thing you can do is put in some **Check** steps to make sure that assumptions you are making about the state of the browser are actually correct.

If all else fails, you can [do some logging](#how-do-i-add-logging-to-my-test?) to figure out what is going on.

### How do I add logging to my test? {#how-do-i-add-logging-to-my-test?}

There’s a **Log** verb for that. **Log** can take any number arguments, and all are sent to log level **INFO**. Since arguments are passed by value, if you want to capture the current state of a variable you should use **std::ref** to wrap a reference to that variable.

```cpp
const int kValueDoesNotChange = 4;
int value_updated_during_test;
RunTestSequence(
  // Do some test steps.
  Log(kValueDoesNotChange,
      “blah blah blah”,
      std::ref(value_updated_during_test)),
  // Do some more test steps.
);
```

You can also add logging to an **AfterShow** or **WithElement** (or similar) if you want to get the state of a specific element.

### How do I see what elements are present at a particular step in the test?

If you’re trying to understand the state of the UI in order to debug a test, you can use the **DumpElements** and **DumpElementsInContext** verbs to pretty-print a full tree of all known contexts and elements to the test log.

We recommend mostly using these for debugging and not checking them in, but like **Log** they may be useful in understanding certain failure cases. Please note that when a test fails, the same output as **DumpElements** is automatically added to the failure log.

### My test can’t find a UI element but I’m sure it’s visible. What should I do? {#my-test-can’t-find-a-ui-element-but-i’m-sure-it’s-visible.-what-should-i-do?}

If you are *absolutely sure* the surface with the element is being displayed, and that the element should be visible, there are still a few common reasons for the test not to find the element.

1. The element identifier isn’t being assigned to the element.
   * The identifier is assigned in one code path but that’s not the code path that’s being executed. Make sure that all the ways the UI element could be created result in the identifier being assigned.
   * The identifier is assigned based on an event, and the event either doesn’t happen or hasn’t happened yet. This can happen especially with help bubble anchors that are dynamically registered by a WebUI.
2. The UI element isn’t the class you thought it was, and didn’t receive the identifier you expected.
   * This often happens when there are deep inheritance hierarchies.
   * For example, a **BrowserAppMenuButton** and **WebAppMenuButton** both derive from **AppMenuButton**; originally, only the **BrowserAppMenuButton** received the **kAppMenuButtonElementId**, which caused webapp tests to not find their app menu buttons.
3. The element is in a different context.
   * It might be a view in a tab-modal dialog.
   * It might be a help bubble anchor in a WebUI.

#### Testing element(s) in another context {#testing-element(s)-in-another-context}

In the cases of (1) and (2), you might need to change the code or the test to assign or refer to the correct identifier. In the case of (3) you will want to use **InAnyContext** (possibly followed by **InSameContext** as necessary):

```cpp
IN_PROC_BROWSER_TEST_F(MyInteractiveTest, Test1) {
  RunTestSequence(
    PressButton(kSidePanelToolbarButtonElementId),
    SelectDropdownItem(kSidePanelDropdownElementId, 0),
    // Because this is a WebUI element, it is in a different context.
    InAnyContext(WaitForShow(kBookmarkSidePanelHelpBubbleAnchorElementId)),
    // Rest of the test goes here.
  );
}
```

**Please note:** When using **InAnyContext**, you must continue to use **InAnyContext** or **InSameContext** for subsequent steps that reference elements in this secondary context. See below for examples.

```cpp
// The following are all equivalent as long as there is only one
// MyTabModalDialog and the element IDs are otherwise unique:

  // 1.
  InAnyContext(
      PressButton(MyTabModalDialog::kApplyButtonElementId),
      WaitForHide(MyTabModalDialog::kDialogElementId))

  // 2.
  InAnyContext(PressButton(MyTabModalDialog::kApplyButtonElementId)),
  InAnyContext(WaitForHide(MyTabModalDialog::kDialogElementId))

  // 3.
  InAnyContext(PressButton(MyTabModalDialog::kApplyButtonElementId)),
  InSameContext(WaitForHide(MyTabModalDialog::kDialogElementId))

// The following, however, will not work because the second step will look
// in the browser window context and not the modal dialog.

  // Bad test code:
  InAnyContext(PressButton(MyTabModalDialog::kApplyButtonElementId)),
  WaitForHide(MyTabModalDialog::kDialogElementId)
```

### My test fails to find an element, but only on some platforms or builders? {#my-test-fails-to-find-an-element,-but-only-on-some-platforms-or-builders?}

This could be one of a few things:

1. There’s a race condition that only manifests on some platforms. Perhaps you need to wait for an additional condition or use **WaitForStateChange**, **ObserveState**, or **PollState** to ensure the application is actually in the state you expect it to be. See [Waiting for State Changes and Events](#waiting-for-state-changes-and-events) below for more information.
2. There’s an implementation difference (perhaps low-level enough you might not have known about it) on different platforms; see below for more information.
3. There’s a genuine behavior difference on different platforms, or a bug that only manifests on one platform. This is unlikely for high-level tests, but might be the case \- be sure to hand test your feature on different platforms to ensure that it actually works the same\!

*Note: you can usually differentiate between “not present” and “present, but in the wrong context” by looking at the dump of all known elements that should be present in the test logs, between the failure description and the stack trace.*

The two types of platform differences are *explicit* differences in implementation in high-level code \- that is, for example, that a particular feature is not enabled or displays differently on different platforms \- and *implicit* differences in low-level code, such as the low-level relationships between Widgets.

If there are explicit differences, you might have to add some **\#if BUILDFLAG** or [conditional](#can-i-perform-test-steps-conditionally?) sections to your test to handle the different platform implementations. This is a normal if unfortunate part of testing.

The two most common implicit differences are where a new browser tab appears (same window or new window) and whether a certain type of popup window is correctly parented to its corresponding browser or application window. In these cases, the new element or page might appear in a new context; try testing them with that in mind ([see here for detailed instructions](#testing-element\(s\)-in-another-context)).

For some popups such as [tab-modal dialogs](#i’m-trying-to-test-a-tab-modal-dialog-but-the-test-can’t-find-the-dialog?) it’s intentional that they have different contexts; for others it’s an oversight in Views that depends on how the Widget is implemented at a low level (the most common popups, bubble dialogs and menus, do not have this problem). We’re currently looking into making this more predictable, but in the meantime, consider making your test more flexible about the context it searches for elements in.

### I’m waiting for an event but it never arrives? {#i’m-waiting-for-an-event-but-it-never-arrives?}

Sometimes you are waiting for an event \- which could be a custom event or something like **WaitForShow().SetTransitionOnlyOnStateChange(true)** \- but the event never comes.

First, verify that the event isn’t being sent. It’s possible that it’s just never triggered in the first place. This can be done with separate logging in the part of the code that would send the event. If you’re waiting for an element to become visible, you can install an additional logging callback directly on **ElementTracker**.

Second, make sure the event isn’t arriving before you start waiting\! Make sure there are no intervening steps between the step that should have triggered the event and actually waiting for the event.

For example, the following code might create a race condition:

```cpp
RunTestSequence(
    // ...
    PressButton(kDoAsyncJobButtonId),
    // Page navigation is asynchronous, so it's possible for the job complete
    // event to come in while we're waiting for the web page to load!
    NavigateWebContents(kPrimaryTabId, kNewUrl),
    // We might or might not get this event, depending on whether the page load
    // or the async job took more time! This is the sort of thing that causes
    // flaky tests!
    WaitForEvent(kAsyncJobCompletedEvent)
    // ...
```

In the above example, it might be better to swap the Navigate and Wait steps. Or if the navigate must happen immediately (for some reason), the two could be done **InParallel**.

Finally, make sure you are listening to the correct element in the correct context.

### I’m trying to test a tab modal dialog but the test can’t find the dialog? {#i’m-trying-to-test-a-tab-modal-dialog-but-the-test-can’t-find-the-dialog?}

Tab-modal dialogs can move between browser windows as their tabs are dragged and therefore don’t belong to the same context; you will need to use **InAnyContext**, optionally followed by **InSameContext**:

```cpp
IN_PROC_BROWSER_TEST_F(MyInteractiveTest, Test1) {
  RunTestSequence(
    // Open a tab-modal dialog.
    InAnyContext(WaitForShow(kTabModalDialogElementId)),
    InSameContext(
        PressButton(kTabModalActionButtonElementId),
        CheckElement(
            kTabModalDisplayElementId,
            // Verification goes here.
        ),
        PressButton(kDialogCloseButtonElementId)
    ),
    // Complete the test.
  );
}
```

## Waiting for State Changes and Events {#waiting-for-state-changes-and-events}

### What types of things does the Kombucha API let me wait for? {#what-types-of-things-does-the-kombucha-api-let-me-wait-for?}

Currently, in addition to an element being shown, hidden, or activated, or a custom event being sent, we can wait for the following:

* For a View metadata property to change; see **WaitForViewProperty()**
  * This is useful if e.g. you need to wait for a button to become enabled before you try to press it.
* For an HTML element to become present in a WebContents and/or for some state to become true; see **WaitForStateChange()**
  * This is useful if e.g. you need to verify that dynamic elements of a page are loaded, or that some asynchronous operation has completed and a WebUI has updated in response.
* For other states, you can create a **StateObserver** and then use **ObserveState()** and **WaitForState().**

There is an example of **WaitForStateChange** [here](#advanced:-probing-web-page-contents) and **ObserveState** [here](#advanced:-waiting-for-non-ui-state). The following is an example of **WaitForViewProperty**:

```cpp
IN_PROC_BROWSER_TEST_F(MyInteractiveTest, Test1) {
  RunTestSequence(
    PressButton(kShowBluetoothControlsButtonElementId),
    // Button may be disabled until Bluetooth service is enabled; wait for
    // enable before trying to press the button.
    WaitForViewProperty(
        kEnableBluetoothButtonElementId, views::View, Enabled, true),
    PressButton(kEnableBluetoothButtonElementId),
    CheckResult(&IsBluetoothEnabled)
  );
}
```

### How do I wait for an event that’s not supported in the Kombucha API? {#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?}

In other words, “I want to transition to the next step when a thing happens, but there is no existing verb for it”. In this case, you have several options, each of which has its own strengths and weaknesses:

1. Use **ObserveState()/WaitForState()** (see [here](#advanced:-waiting-for-non-ui-state) for usage).
   * Advantages: extremely simple test syntax.
   * Disadvantages:
     * Have to write your own **StateObserver** class.
     * Handles state, not discrete events.
2. Set up a listener for the event as part of your test, send a custom event from the listener, and use **WaitForEvent()**.
   * Advantages: your production code doesn’t have to know about your test event.
   * Disadvantages:
     * Doesn’t generalize to other tests or test fixtures.
     * You have to be waiting for the event precisely when it will be sent or you might miss it. (This can be mitigated with parallelism.)
3. Have your system emit a custom event whenever the event you care about happens, and then use **WaitForEvent()**.
   * Advantages:
     * Clean approach with very little test boilerplate.
     * All tests have access to the event, not just the one you’re currently writing.
     * Can use the event in Tutorials in addition to tests.
   * Disadvantages:
     * Have to add a test-specific event to production code.
     * You have to be waiting for the event precisely when it will be sent or you might miss it. (This can be mitigated with parallelism.)
4. Embed a run loop in a step callback to wait for the event.
   * Advantages: simple to implement.
   * Disadvantages:
     * Embedded run loops create opportunities for timeouts and deadlocks that Kombucha tests normally avoid.
     * Embedded run loops can’t be run in parallel if you need to wait for multiple events.

Here is an example of (2) \- adding a listener and sending an event. Note the use of a **TestElement** as a relay. Also note that since an event step can be triggered during the previous step, there is no need to check to see if the event has already happened.

```cpp
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kEventElementIdentifier);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kDoAThingCompleteEvent);
TestElement el(
    kEventElementIdentifier, BrowserElements::From(browser())->GetContext());
el.Show();
CallbackListSubscription subscription =
    DoAThingSystem::AddOnthingDoneCallback(base::BindLambdaForTesting(
      [&](){ el.NotifyEvent(kDoAThingCompleteEvent); }
    ));

RunTestSequence(
  PressButton(kDoAThingButtonElementId),
  WaitForEvent(kEventElementIdentifier, kDoAThingCompleteEvent)
);
```

Here’s an example of (3) \- always sending an event when the event happens. Note how, because the code in DoAThingSystem only refers to shared identifiers and events (which could be declared in the DoAThingSystem class or in **browser\_element\_identifiers**), and on the **ElementTracker**, it does not need to depend on Views.

```cpp
// In do_a_thing_system.cc:
void DoAThingSystem::OnThingDone() {
  const auto* el =
      ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          BrowserElements::From(browser_)->GetContext(),
          kDoAThingButtonElementId);
  if (el) {
    ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        el, kDoAThingCompleteEvent);
  }
}

// In do_a_thing_system_browsertest.cc:
RunTestSequence(
  PressButton(kDoAThingButtonElementId),
  WaitForEvent(kDoAThingButtonElementId, kDoAThingCompleteEvent)
);
```

Here is an example of (4) \- adding a run loop. Note that any nested run loop *must be nestable* or your test could hang.

```cpp
RunTestSequence(
  PressButton(kDoAThingButtonElementId),
  Do([](){
    if (!DoAThingSystem::IsThingDone()) {
      // Make sure the loop is nestable, otherwise there is a much higher
      // chance of deadlocks!
      RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
      auto subscription = DoAThingSystem::AddOnThingDoneCallback(
          run_loop.QuitClosure());
      run_loop.Run();
    }
  })
);
```

### How do I wait for an asynchronous task to complete? {#how-do-i-wait-for-an-asynchronous-task-to-complete?}

The same approaches can be used for asynchronous tasks as for events; [see the previous question](#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?) for more details.

### How do I wait for a View to get into an expected state?

Perhaps you have a View that has a public getter or full-fledged class property, and you want to ensure it eventually settles in some expected state.
If it’s just a getter, you can use **PollViewProperty**:

```cpp
DEFINE_LOCAL_POLLING_VIEW_PROPERTY_STATE_IDENTIFIER(
   DoAThingButton, is_highlighted, kHighlightedState);

RunTestSequence(
  PressButton(kDoAThingButtonElementId),
  PollViewProperty(kHighlightedState, kDoAThingButtnoElementId),
  WaitForState(kHighlightedState, true)
);
```

If it’s a full-fledged property (with a getter and a subscription-based changed callback that exactly follows Views conventions), you can use:

```cpp
RunTestSequence(
  PressButton(kDoAThingButtonElementId),
  WaitForViewProperty(
        kDoAThingButtonElementId, DoAThingButton, Highlighted, true)
);
```

Note that if there is a subscription-based change callback but the naming convention of the getter and/or add callback method are not consistent, you should change them so they are, but if that’s not possible, you can use **WaitForViewPropertyCallback**.

Finally, there is also **PollView** when the condition you are polling can’t easily be retrieved from a  property; this is similar to **PollElement**.

### How do I wait for a Views animation to complete? {#how-do-i-wait-for-a-views-animation-to-complete?}

The same approaches can be used for animations as for events; [see the previous question](#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?) for more details.

However, here is a general outline of something that *should* work:

1. Make sure the view or container that is animating has an element identifier.
2. Define an “animation complete” custom event.
3. On animation complete, emit the event.
4. In your test, wait for the event.

```cpp
class MyView : public views::View {
 public:
  ...
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kAnimationCompleted);
  ...
};

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(MyView, kAnimationCompleted);

MyView::MyView() {
  ...
  SetProperty(views::kElementIdentifierKey, kMyViewElementId);
}

void MyView::OnAnimationComplete(...) {
  ...
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kAnimationCompleted, this);
}

IN_PROC_BROWSER_TEST_F(MyViewTest, TestAnimate) {
  RunTestSequence(
    ...
    DoThingThatTriggersAnimation(),
    WaitForEvent(kMyViewElementId, MyView::kAnimationCompleted),
    ...
}
```

### I have events that can happen in different orders \- how do I avoid a race condition? {#i-have-events-that-can-happen-in-different-orders---how-do-i-avoid-a-race-condition?}

If you have multiple [events](#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?) or [asynchronous tasks](#how-do-i-wait-for-an-asynchronous-task-to-complete?) (see those sections for more information) you want to wait for but which might arrive in different orders, you should use the **InParallel** modifier. Note that **InParallel** can take any number of arguments, each of which can be a single step or a **Steps**, and each argument is executed in parallel.

Caveats:

* First, ask yourself if one or both of these are \[non-transient\] states that can be observed with **StateObserver** rather than discrete events; if so, you can observe both and wait in any order.
* Be careful not to do anything in one branch that could interfere with a check or action in a different branch\!
* Be careful when using a **RunLoop** in any parallel branch, as multiple run loops could cause the branches to deadlock \- [prefer sending events instead](#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?).

Example:

```cpp
RunTestSequence(
  PressButton(kDoAThingButtonElementId),
  InParallel(
    RunSubsequence(
        WaitForEvent(kDoAThingButtonElementId, kDoAThingCompleteEvent)),
    RunSubsequence(
        WaitForShow(kDoAThingCompleteDialogElementId)
            .SetTransitionOnlyOnEvent(true),
        PressButton(kDoAThingCompleteDialogCloseButtonElementId)
    )
  ),
  // The following steps will only execute after both branches of the
  // InParallel complete.
  // ...
);
```

### I have events that will happen simultaneously; how do I wait for all of them? {#i-have-events-that-will-happen-simultaneously;-how-do-i-wait-for-all-of-them?}

For state changes, like an element becoming visible or hidden, or a change to a web page, you can probably just check them in any order; the conditions will each become true at some point and stay true throughout.

However, for discrete events, you will want to use an **InParallel** directive to avoid race conditions due to variable event ordering. [See the here for more information](#i-have-events-that-can-happen-in-different-orders---how-do-i-avoid-a-race-condition?) on how to use **InParallel**.

### My test gets stuck in a **RunLoop** \- help\! {#my-test-gets-stuck-in-a-runloop---help!}

First off, if you’re doing drag-drop, contact the Kombucha team; there are some very specific platform considerations that are still being resolved (especially on Windows).

Otherwise, getting stuck in a **RunLoop** tends to be the result of some other system creating a non-nestable run loop *inside* of the test’s run loop. This in turn can cause a deadlock or prevent the test from receiving messages or advancing properly.

The first thing to do when you are getting stuck in a loop is to find the actual declaration of the loop and add **RunLoop::Type::kNestableTasksAllowed** to the declaration; see if that fixes the problem.

The second thing to do is, if you are depending on a RunLoop in a non-Kombucha test library to detect some event or state change, see if there’s another [Kombucha-friendly way to listen for the event](#how-do-i-wait-for-an-event-that’s-not-supported-in-the-kombucha-api?) and use that approach instead.

Next, if that doesn’t work, see if there’s some order of operations issue that’s creating a deadlock. It’s possible you could re-order your test steps.

Finally, *contact the Kombucha team*\! We may have a suggestion you haven’t thought of.

## Testing Web Pages and WebUI {#testing-web-pages-and-webui}

### I need to check the contents of a web page \- how do I do that? {#i-need-to-check-the-contents-of-a-web-page---how-do-i-do-that?}

You can use **InstrumentTab** to “instrument” a web page. This does several things:

* Assigns an element identifier to the page, which becomes “hidden” during navigation and “shown” when the page is ready.
* Lets you use the **WaitForWebContentsReady** and **NavigateWebPage** verbs to control and check for navigation.
* Lets you use the **WaitForStateChange** verb to check or wait for page state.
* Lets you refer to specific elements in the page via **DeepQuery**.

Note that an **InstrumentTab** test step will not complete until the page is ready, so you do not have to immediately follow up with **WaitForWebContentsReady**.

Also note that you’ll want to declare a unique identifier to assign to the page. Identifiers can be shared between tests but should not be reused in the same test.

Example:

```cpp
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMyTabElementId);

RunTestSequence(
  InstrumentTab(kMyTabElementId),
  CheckJsResult(
      kMyTabElementId,
      // Put an interesting JavaScript check function here...
  ),
);
```

### What is a DeepQuery? How do they work? {#what-is-a-deepquery?-how-do-they-work?}

A **DeepQuery** is a way to navigate a page’s shadow DOM to find a specific element. A deep query is one or more segments that are evaluated as follows:

1. Start with the document element.
2. Run querySelector on the next segment from the current element.
3. If this is the last segment, return the result.
4. If the element hosts a shadow DOM, jump to its root and return to (2).

So, for example, if a deep query is \[“my-web-app”, “\#navigation-pane a:nth-of-type(2)”\], it will find the “my-web-app” element, jump into its shadow DOM, and then find the second anchor that is a descendant of the element with id “navigation-pane”.

Deep queries are used by a number of verbs that access or check the contents of instrumented WebContents.

### How do I instrument a web page that I’m about to open? {#how-do-i-instrument-a-web-page-that-i’m-about-to-open?}

If you’re going to navigate an existing tab, you can instrument the tab and then either use **NavigateWebPage** to navigate to the page you want or trigger the navigation some other way and use **WaitForWebContentsNavigation**. You should not use **WaitForWebContentsReady** because it might introduce a race condition \- is the existing page ready, or the new page?

If you intend to do something that will cause a new tab in the current or another browser to open, then you want **InstrumentNextTab**. This verb also lets you specify which browser you expect the tab to open in.

### How do I navigate to a specific web page? {#how-do-i-navigate-to-a-specific-web-page?}

Use **NavigateWebPage**, or do some browser action that would result in the page navigating and use **WaitForWebContentsNavigation**. The former directly navigates and waits for the page; the latter assumes that the page will navigate and the test will time out if the page fails to do so.

### I want to open a page using the omnibox \- how do I do that? {#i-want-to-open-a-page-using-the-omnibox---how-do-i-do-that?}

Use **EnterText** to set the omnibox text and then **Confirm** to commit:

```cpp
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMyTabElementId);

RunTestSequence(
  InstrumentTab(kMyTabElementId),
  EnterText(kOmniboxElementId, kNewUrl),
  Confirm(kOmniboxElementId),
  WaitForWebContentsNavigation(kMyTabElementId, GURL(kNewUrl))
);
```

### How do I instrument a WebUI that isn’t in a tab? {#how-do-i-instrument-a-webui-that-isn’t-in-a-tab?}

Use **InstrumentNonTabWebView**. You will need the **WebView** containing the WebUI to either have its own identifier, or assign a name via a **NameView** or similar call. By default, the instrument step will not complete until the WebUI is ready.

### Can I instrument a page that isn’t a **chrome://** page or WebUI? {#can-i-instrument-a-page-that-isn’t-a-chrome://-page-or-webui?}

Yes, you can instrument any web page, not just WebUI or chrome:// pages.

### Should I use **MoveMouseTo**/**ClickMouse** or inject an **element.click()** to press a button? {#should-i-use-movemouseto/clickmouse-or-inject-an-element.click()-to-press-a-button?}

Just like with [**PressButton**](#should-i-use-pressbutton-or-movemouse/clickmouse?), it’s fine to use an **ExecuteJsAt** with a function that calls “Element.click()” unless you need to actually inject mouse input. There are some page- or system-specific reasons to use simulated mouse input instead of just calling “click” but that will be very specific to the test in question.

In the future we may add a version of **PressButton** that takes a deep query, to make this even easier.

### Can I send keyboard accelerators to a web page? {#can-i-send-keyboard-accelerators-to-a-web-page?}

Yes. You may need to **FocusWebContents()** to ensure that the page is ready to receive the accelerators, then you can use **SendAccelerator()** targeting either the Instrumented WebContents element *or* the WebView containing the WebContents. If the page is in a background tab, you will need to do a **SelectTab()** on the tabstrip (or bring the tab to the front in some other way) before attempting to focus it.

### Can I send keyboard accelerators to dialog? {#can-i-send-keyboard-accelerators-to-dialog?}

Yes. Make sure the dialog is focused, and use **SendAccelerator()** to send the accelerators to the dialog. If your dialog has a **WebContents**, be sure to [instrument the web contents](#how-do-i-instrument-a-webui-that-isn’t-in-a-tab?) and then send the accelerator to the instrumented element; e.g.:

```cpp
RunTestSequence(
  // ...
  PopUpMyDialog(),
  InstrumentWebContents(kMyDialogWebContents, kMyDialogWebView),
  SendAccelerator(kMyDialogWebContents, accel),
```

This ensures that if the keypress should be handled by the WebContents and not the dialog itself, it is.

Do not send accelerators to the browser view that are meant for a dialog, as they will not be routed correctly on most platforms.

### How do I check element properties in a web page? {#how-do-i-check-element-properties-in-a-web-page?}

Use **CheckJsResultAt**. This finds an element via a **DeepQuery** and then executes a JavaScript function on it; the function must be of the form: “(*element*) \=\> *something that returns a value*”. You can then specify a **Matcher** that will evaluate against the result; if you don’t then the step succeeds if the resulting value is truthy.

Note that the body of your function can be arbitrarily complex and potentially return asynchronous results; the check will complete when the function does.

If you want to wait for an element property to achieve a particular value, you can use [**WaitForStateChange**](#what-is-waitforstatechange-for?) instead.

### How do I check javascript variables in a web page? {#how-do-i-check-javascript-variables-in-a-web-page?}

Use **CheckJsResult**. This evaluates a JavaScript function of the form: “() \=\> *something that returns a value*”. You can then specify a **Matcher** that will evaluate against the result; if you don’t then the step succeeds if the resulting value is truthy.

Note that the body of your function can be arbitrarily complex and potentially return asynchronous results; the check will complete when the function does.

If you want to wait for a JavaScript variable to achieve a particular value, you can use [**WaitForStateChange**](#what-is-waitforstatechange-for?) instead.

### What is **WaitForStateChange** for? {#what-is-waitforstatechange-for?}

**WaitForStateChange** halts test execution until a condition becomes true in a WebContents. The following scenarios are supported:

* A specific element is added to the DOM.
* A function you specify returns true.
* A function when applied to a specific element in the DOM returns true.
* A specific element becomes present in the DOM *and* a function you specify returns true when applied to that element.
* One of the above *doesn’t* happen in a prescribed amount of time.

This allows you to wait for nearly any state in a web page (or ensure it does not occur), as long as it can be queried by a JavaScript function executed in the correct environment. The browser continues to run while you are waiting, and a timeout will occur if the wait fails.

Pay special attention to the **StateChange::Type** enumeration, which details the types of conditions you can wait for. Also note that if you do not specify **StateChange::where**, the function you supply will be applied at the global scope and take no arguments.

Here’s an example; note that you must define a specific custom event to use as a trigger:

```cpp
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMyConditionMetEvent);

StateChange expected_state;
expected_state.type = StateChange::Type::kExistsAndConditionTrue;
expected_state.where = {“my-web-app”, “#statusMessage”};
expected_state.test_function = “el => el.innerText === ‘success’”;
expected_state.event = kMyConditionMetEvent;

RunTestSequence(
  InstrumentTab(kMyTabElementId),
  ExecuteJs(kMyTabElementId, kPathToButton, “el => el.click()”),
  WaitForStateChange(kMyTabElementId, expected_state)
);
```

### How do I wait for a web page to update? {#how-do-i-wait-for-a-web-page-to-update?}

To wait for an expected update to a page after some interaction, use [**WaitForStateChange**](#what-is-waitforstatechange-for?) to determine when the page has achieved the expected state.

### How do I deal with lazy-loaded web elements? {#how-do-i-deal-with-lazy-loaded-web-elements?}

To wait for lazy-loaded components to appear, use [**WaitForStateChange**](#what-is-waitforstatechange-for?) to determine when the page has loaded the expected components (and optionally that they have achieved the expected state).

### I’m trying to check a WebUI element but sometimes it fails because the element isn’t there? {#i’m-trying-to-check-a-webui-element-but-sometimes-it-fails-because-the-element-isn’t-there?}

It’s possible that the element is being dynamically created. You can use [**WaitForStateChange**](#what-is-waitforstatechange-for?) to ensure that the page has sufficient time to create and populate the element you expect.

Also, if the element is consistently not there, you may want to inspect the page using Developer Tools in Chrome; it’s possible that you are providing the wrong DeepQuery, or that the path to the element may vary (this is especially the case if you are using any of the “nth\*” selectors and the number of elements may vary).
If, for example, you are looking for a specific value in a list, but the list could be in different orders, you probably want to do a **WaitForStateChange** or **CheckJsResultAt** that targets the list itself rather than a specific list element, and manually searches the list items for the one you care about.

## Testing User Education Experiences {#testing-user-education-experiences}

### How do I test my IPH or Tutorial? {#how-do-i-test-my-iph-or-tutorial?}

Help bubbles have built-in identifiers for the bubble dialog, default button, and first non-default button, in addition to potentially other elements. You can perform the triggering actions and then verify that the expected bubble appears using **WaitForShow** or **CheckView**.

There are a lot of issues that can prevent an IPH from showing, and a lot of prerequisites that are required for testing IPH. Therefore we have provided **InteractiveFeaturePromoTest\[API|T\]** to avoid these issues. By inheriting from these classes instead of **InteractiveBrowserTest**, you will have finer control over these factors along with a bunch of helpful custom verbs.

Note that if you do not inherit from **InteractiveFeaturePromoTest\***, you will need to set up a [ScopedIPHFeatureList](https://source.chromium.org/chromium/chromium/src/+/main:components/feature_engagement/test/scoped_iph_feature_list.h;l=25) in the test **SetUp** method, as IPH are disabled by default in browser and interactive tests to avoid accidentally triggering during unrelated tests. (And, as always, when overriding **SetUp**, **SetUpOnMainThread**, or **TearDownOnMainThread**, don’t forget to call the super-class implementation\!)

### How do I test a WebUI IPH or Tutorial? {#how-do-i-test-a-webui-iph-or-tutorial?}

You can instrument the WebContents, perform the triggering condition, and then use **WaitForStateChange** to ensure that the **\<help-bubble\>** element appears in the expected location in the document. The placement for help bubbles is as follows:

* For WebUI that appears in a tab (such as Settings), placement is in:
  * The **offsetParent** of the anchor, or…
  * The **shadowRoot** of the Polymer element that registered the help bubble anchor if the anchor position is fixed or the Polymer element is nested inside the anchor’s **offsetParent**.
* For WebUI that appears elsewhere in the Chrome UI, a Views help bubble is displayed that floats with the target WebUI element; [see here](#how-do-i-test-my-iph-or-tutorial?) for more information on testing Views help bubbles.

### I’m trying to test my WebUI IPH/Tutorial but the test can’t find an anchor? {#i’m-trying-to-test-my-webui-iph/tutorial-but-the-test-can’t-find-an-anchor?}

Remember that help bubble anchors are not in the browser context, but rather in their own context associated with the render process. Use **InAnyContext** and, if necessary, **InSameContext** to locate the anchor element and help bubble.

If you instrument a tab in order to poke at its contents, then the help bubble anchors and the instrumented **WebContents** will be in different contexts (the latter will be in the browser context).

## Advanced Test Construction {#advanced-test-construction}

### Can I procedurally generate test steps? {#can-i-procedurally-generate-test-steps?}

Yes. You do not have to create all your test steps inside of the call to **RunTestSequence**.

You can create your own **MultiStep** and add test steps to it using **operator \+=**, or create it all at once with **Steps**. It can then be passed into RunTestSequence as either part or all of the test sequence to run.

This is similar to how [custom verbs](#custom-verbs) work.

For example:

```cpp
IN_PROC_BROWSER_TEST_F(MyInteractiveTest, Test1) {
  auto test_steps = Steps(
      InstrumentTab(kMainTabId),
      ClickButton(kMyFeatureEntryPointId));

  if (base::FeatureList::IsEnabled(features::kMyFeatureInterstitialMenu)) {
    test_steps += SelectMenuItem(kOpenMyFeatureMenuItemElementId);
  }

  test_steps += WaitForWebContentsNavigation(
      kMainTabId, GURL(“chrome://my-feature”)));

  RunTestSequence(std::move(test_steps));
}
```

This is less clear than doing the entire test within RunTestSequence (with a custom verb or [conditional step](#can-i-perform-test-steps-conditionally?) where necessary), so prefer not to pre-create test sequences in your test code unless it’s absolutely necessary.

### Can I perform test steps conditionally? {#can-i-perform-test-steps-conditionally?}

Yes\! There are two options:

1. If this is a condition that is known when the test is run (i.e. something about the test platform, environment, or OS, a **GetParam** value, etc.) you can create a [custom verb](#custom-verbs) that selectively generates the correct steps, or just conditionally add the steps in the test body.
2. If this is a condition that is only available during test execution, then you can use a *conditional* such as **If**, **IfResult**, **IfView**, etc.

Note that tests should almost always be deterministic, so conditionals (option 2\) should usually be avoided unless they significantly improve readability. Here are some potential alternatives:

* If you need to do something differently based on whether some asynchronous operation or notification has happened yet, consider [explicitly waiting for its completion](#waiting-for-state-changes-and-events).
* If a race condition cannot be avoided due to potential differences in the order of operations, consider [running two subsequences in parallel](#i-have-events-that-can-happen-in-different-orders---how-do-i-avoid-a-race-condition?).
* If the browser’s behavior itself is non-deterministic, consider whether there is a bug or poorly designed system causing the problem, and whether you should be writing your test to work around that issue or not.

Here’s an example of a conditional step using a custom verb:

```cpp
auto MaybeCheckHistogram(int expected_count) {
  // The histogram will only be populated if GetParam() is true, so only
  // check it if it’s true.
  auto steps = GetParam() ?
      Steps(Check(&GetHistogramValue, expected_count)) :
      Steps();
  AddDescriptionPrefix(steps, "MaybeCheckHistogram()");
  return steps;
}

RunTestSequence(
    DoCustomOperation(),
    MaybeCheckHistogram(/*expected_count*/ 4));
```

Note: if your custom verb needs to do other stuff, you can [nest your conditional call to **Steps**](#nested-steps\(\)) inside another call to **Steps**.

Here’s the same example, but inlining the step generation logic:

```cpp
RunTestSequence(
    DoCustomOperation(),
    // The histogram will only be populated if GetParam() is true, so only
    // check it if it’s true.
    GetParam() ?
        Steps(Check(&GetHistogramValue, 4)) :
        Steps());
```

Here is the same example using an **If** statement:

```cpp
RunTestSequence(
    DoCustomOperation(),
    // The histogram will only be populated if GetParam() is true, so only
    // check it if it’s true.
    If([this](){ return GetParam(); },
        Then(Check(&GetHistogramValue, 4))))
```

The biggest difference between the above options is that the **If** statement is evaluated *as the sequence is running*, while the others are evaluated when the sequence is constructed. So if you don’t know what’s going to happen until you get to the conditional step, you must use an **If** or similar statement.

### Can I call RunTestSequence more than once in the same test? {#can-i-call-runtestsequence-more-than-once-in-the-same-test?}

Yes\!

It’s not best practice, but some types of tests are easier to write this way (especially if you’ve got to compute some parameters for the second sequence after the first completes).

## General Testing Issues {#general-testing-issues}

### I need to load a web page but it isn’t working\! {#i-need-to-load-a-web-page-but-it-isn’t-working!}

Did you remember to call the following on startup?

* **embedded\_test\_server()-\>Start()**
* **host\_resolver()-\>AddRule()** (if applicable)

Note that if you override **SetUpOnMainThread**, you will need to call the base-class implementation as party of your overridden method.

Also, remember to call **embedded\_test\_server()-\>GetURL()** in your test to calculate the actual URL you need to navigate to.

Failure to do any of the above may result in a page not loading properly as part of a **NavigateWebPage** or omnibox navigation.

### How do I root cause test flakes (especially on CQ/CI)? {#how-do-i-root-cause-test-flakes-(especially-on-cq/ci)?}

Occasional flakes are *not* normal and suggest a bug or race condition in either your test or the underlying libraries (production or test).

*We do not guarantee that there are no bugs in the testing library, just that we intend to eventually fix all of the bugs that are found\!*

The first thing you should do is look at the log output of the failing test. If the issue is a known one, there will likely be a lengthy text block detailing the known incompatibility and offering suggestions on how to get around it. (However, this assumes you can generate the error reliably and look at the log output.)

#### Reproducing Flakes {#reproducing-flakes}

If a flake occurs in a test environment you can replicate locally, you can use the **–gtest\_repeat** flag when running the test from the command line, to increase the likelihood of catching the error. A value of 20-50 will catch most common flakes.

If a flake occurs in a test environment that you cannot reproduce locally but a buildbot is available for, there is a rather delightful trick for reproducing it. Turn your test into a parameterized test using a dummy range of the appropriate repeat count, as follows:

```cpp
class MyFeatureUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<int> { ... };

INSTANTIATE_TEST_SUITE_P(,
    WebUITabStripDragInteractiveTest, testing::Range(0, 50));

IN_PROC_BROWSER_TEST_P(MyFeatureUiTest, PossiblyFlakyTest) { ... }
```

This will run the test (in this example) 50 times. You can then upload the test and run it on whatever buildbot is likely to flake, including CQ bots and many CI bots as well.

#### Root-Causing Flakes {#root-causing-flakes}

Once you have reproduced the flake, it’s then necessary to root-cause the problem. Hopefully, you will be able to see which step failed, and then figure out what went wrong. If it’s not immediately obvious, the next step would be to put in a bunch of additional [**Check steps**](#how-do-i-perform-checks-in-my-test?) to verify your assumptions about the state of the browser, and/or a bunch of [additional logging](#how-do-i-add-logging-to-my-test?).

If you still can’t figure out what is wrong, feel free to contact us; we’ll work with you to locate the problem. In the meantime, you can potentially disable the test on the platform you know doesn’t work.

#### What if the bug is in the test framework itself? {#what-if-the-bug-is-in-the-test-framework-itself?}

*File a bug* and contact us immediately. If it’s a simple bug to fix, we’ll be happy to do so. If it’s a complicated issue with low-level libraries, we’re probably already working on a solution but it might not be immediately forthcoming. Googlers can use [go/kombucha-bug](http://goto.google.com/kombucha-bug) to get an easy template.

In the meantime, you can probably disable the test on the offending platform, or insert a hand-rolled step with some code you know will work correctly and a TODO with the bug you filed and the code you wanted to use; when the bug is fixed we’ll replace it with the correct Kombucha logic.

*Please bear with us*; not everything is testable yet via the same automated test on every platform, due to underlying differences in how windowing, events, etc. are handled. It’s important to identify what can be automated by tests that are not Kombucha (which we should be able to then fix) and what is not automatable at all with existing libraries (which we might still be able to fix in the future, but which will probably require more work).

Googlers can reach out with further questions or concerns at [go/​​kombucha-chat](http://go/kombucha-chat).