// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTION_SEQUENCE_BROWSER_UTIL_H_
#define CHROME_TEST_INTERACTION_INTERACTION_SEQUENCE_BROWSER_UTIL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace views {
class WebView;
}

class Browser;

class TrackedElementWebPage;

// This is a test-only utility class that wraps a specific WebContents in a
// Browser for use with InteractionSequence. It allows tests to:
//  - Treat pages loaded into a specific WebContents as individual
//    ui::TrackedElement instances, including responding to pages loads and
//    unloads as show and hide events that can be used in a sequence.
//  - Navigate between pages in a given WebContents.
//  - Inject and execute javascript into a loaded page, and observe any return
//    value that results.
//  - Wait for a condition (evaluated as a JS statement or function) to become
//    true and then send a custom event.
//  - Track when a WebContents is destroyed or removed from a browser window.
class InteractionSequenceBrowserUtil : private content::WebContentsObserver,
                                       private TabStripModelObserver {
 public:
  // How often to poll for state changes we're watching; see
  // SendEventOnStateChange().
  static constexpr base::TimeDelta kDefaultPollingInterval =
      base::Milliseconds(200);

  // Series of elements to traverse in order to navigate through the DOM
  // (including shadow DOM). The series is traversed as follows:
  //  * start at document
  //  * for each selector in deep_query
  //    - if the current element has a shadow root, switch to that
  //    - navigate to the next element of deep_query using querySelector()
  //  * the final element found is the result
  using DeepQuery = std::vector<std::string>;

  // Specifies a state change in a web page that we would like to poll for.
  // By using `event` and `timeout_event` you can determine both that an
  // expected state change happens in the expected amount of time, or that a
  // state change *doesn't* happen in a particular length of time.
  struct StateChange {
    StateChange();
    StateChange(StateChange&& other);
    StateChange& operator=(StateChange&& other);
    ~StateChange();

    // What type of state change are we watching for?
    enum class Type {
      // Triggers when `test_function`, evaluated at `where`, returns true.
      // If `where` does not exist, an error is generated.
      kConditionTrue,
      // Triggers when the element specified by `where` exists in the DOM.
      // The `test_function` field is ignored.
      kExists,
      // Triggers when the element specified by `where` exists in the DOM *and*
      // `test_function` evaluates to true.
      kExistsAndConditionTrue
    };

    // By default we want to check `test_function` and assume the element
    // exists.
    Type type = Type::kConditionTrue;

    // Function to be evaluated every `polling_interval`. Must be able to
    // execute multiple times successfully. State change is detected when this
    // script returns a "truthy" value.
    //
    // Must be in the form of an unnamed function, e.g.:
    //  function() { return window.valueToPoll; }
    // or:
    //  () => document.querySelector(#my-label).innerText()
    std::string test_function;

    // If specified, the series of selectors to find the element you want to
    // perform the operation on. If not empty, test_function should take a
    // single DOM element argument, e.g.:
    //  el => el.innerText == 'foo'
    //
    // If you want to simply test whether the element at `where` exist, you may
    // leave `test_function` blank.
    DeepQuery where;

    // How often to poll. `test_script` is not run until this elapses once, so
    // a longer interval will extend the duration of the test.
    base::TimeDelta polling_interval = kDefaultPollingInterval;

    // How long to wait for the condition before timing out. If not set, waits
    // indefinitely (in practice, until the test itself times out).
    absl::optional<base::TimeDelta> timeout;

    // The event to fire when `test_script` returns a truthy value. Must be
    // specified.
    ui::CustomElementEventType event;

    // The event to fire if `timeout` is hit before `test_script` returns a
    // truthy value. If not specified, generates an error on timeout.
    ui::CustomElementEventType timeout_event;
  };

  ~InteractionSequenceBrowserUtil() override;

  // Creates an object associated with a WebContents in the Browser associated
  // with `context`. The TrackedElementWebContents associated with loaded pages
  // will be created with identifier `page_identifier` but you can later change
  // this by calling set_page_identifier(). If `tab_index` is specified, a
  // particular tab will be used, but if it is not, the active tab is used
  // instead.
  static std::unique_ptr<InteractionSequenceBrowserUtil>
  ForExistingTabInContext(ui::ElementContext context,
                          ui::ElementIdentifier page_identifier,
                          absl::optional<int> tab_index = absl::nullopt);

  // As above, but you may directly specify the Browser to use.
  static std::unique_ptr<InteractionSequenceBrowserUtil>
  ForExistingTabInBrowser(Browser* browser,
                          ui::ElementIdentifier page_identifier,
                          absl::optional<int> tab_index = absl::nullopt);

  // Creates a util object associated with a WebContents, which must be in a
  // tab. The associated TrackedElementWebContents will be assigned
  // `page_identifier`.
  static std::unique_ptr<InteractionSequenceBrowserUtil> ForTabWebContents(
      content::WebContents* web_contents,
      ui::ElementIdentifier page_identifier);

  // Creates a util object associated with a WebView in a secondary UI (e.g. the
  // touch tabstrip, tab search box, side panel, etc.) The associated
  // TrackedElementWebContents will be assigned `page_identifier`.
  static std::unique_ptr<InteractionSequenceBrowserUtil> ForNonTabWebView(
      views::WebView* web_view,
      ui::ElementIdentifier page_identifier);

  // Creates a util object that becomes valid (and creates an element with
  // identifier `page_identifier`) when the next tab is created in the Browser
  // associated with `context` and references that new WebContents.
  static std::unique_ptr<InteractionSequenceBrowserUtil> ForNextTabInContext(
      ui::ElementContext context,
      ui::ElementIdentifier page_identifier);

  // Creates a util object that becomes valid (and creates an element with
  // identifier `page_identifier`) when the next tab is created in `browser`
  // and references that new WebContents.
  static std::unique_ptr<InteractionSequenceBrowserUtil> ForNextTabInBrowser(
      Browser* browser,
      ui::ElementIdentifier page_identifier);

  // Creates a util object that becomes valid (and creates an element with
  // identifier `page_identifier`) when the next tab is created in any browser
  // and references the new WebContents.
  static std::unique_ptr<InteractionSequenceBrowserUtil> ForNextTabInAnyBrowser(
      ui::ElementIdentifier page_identifier);

  // Returns the browser that matches the given context, or nullptr if none
  // can be found.
  static Browser* GetBrowserFromContext(ui::ElementContext context);

  // Returns whether the given value is "truthy" in the Javascript sense.
  static bool IsTruthy(const base::Value& value);

  // Takes a screenshot based on the contents of `element` and compares with
  // Skia Gold. Not all element types may be supported. On platforms where
  // screenshots are unsupported or flaky, may trivially return true.
  //
  // If `element` is a TrackedElementWebPage that corresponds to a tab, the tab
  // must be the active tab in the browser window.
  //
  // The name of the screenshot will be composed as follows:
  //   TestFixture_TestName[_screenshot_name]_baseline
  // If you are taking more than one screenshot per test, then `screenshot_name`
  // must be specified and unique within the test; otherwise you may leave it
  // empty.
  //
  // IMPORTANT USAGE NOTES:
  //
  // In order to actually take screenshots:
  // - Your test must be in browser_tests rather than interactive_ui_tests
  // - Your test must be included in pixel_browser_tests.filter
  //
  // Note that test in browser_tests (when not running in the
  // pixel_browser_tests CQ task) may run at the same time as other tests, which
  // can result in flakiness for interaction tests (especially if mouse
  // position, window activation, or occlusion could change the behavior of a
  // test). So if you need to both test complex interaction and take screenshots
  // you have several options:
  //  1. Make a detailed test for interactive_ui_tests and one or more simple
  //     tests that just verify the UI visuals in browser_tests (downside: code
  //     duplication)
  //  2. Put a test in browser_tests that only runs when the command line flag
  //     for pixel tests is set (downside: won't run on platforms that don't
  //     support pixel tests)
  //  3. Put the full test in browser_tests and harden it against activation and
  //     focus changes, mouse position, etc. - e.g. by using things like
  //     BubbleDialogDelegate::PreventCloseOnDeactivate() (downside: more
  //     complicated test, still potential for flaking)
  //
  // In general, if (3) is possible and you can be sure your test won't flake,
  // it's probably the best choice, followed by (1) if you can't guarantee
  // stability in non-single-process tests.
  //
  // We are currently considering enabling pixel tests in interactive_ui_tests,
  // which would solve the problem by providing a single safe place for both.
  static bool CompareScreenshot(ui::TrackedElement* element,
                                const std::string& screenshot_name,
                                const std::string& baseline);

  // Allow access to the associated WebContents.
  content::WebContents* web_contents() const {
    return WebContentsObserver::web_contents();
  }

  // Gets or sets the identifier to be used for any pages subsequently loaded
  // in the target WebContents. Does not affect the current loaded page, so set
  // before initiating navigation.
  ui::ElementIdentifier page_identifier() const { return page_identifier_; }
  void set_page_identifier(ui::ElementIdentifier page_identifier) {
    page_identifier_ = page_identifier;
  }

  // Returns if the current page is loaded. Prerequisite for calling
  // Evaluate() or SendEventOnStateChange().
  bool is_page_loaded() const { return current_element_ != nullptr; }

  // Page Navigation ///////////////////////////////////////////////////////////

  // Loads a page in the target WebContents. The command must succeed or an
  // error will be generated.
  //
  // Does not block. If you want to wait for the page to load, you should use an
  // InteractionSequence::Step with SetType(kShown) and
  // SetTransitionOnlyOnEvent(true).
  void LoadPage(const GURL& url);

  // Loads a page in a new tab in the current browser. Does not block; you can
  // wait for the subsequent kShown event, etc. to determine when the page is
  // actually loaded. The command must succeed or an error will be generated.
  //
  // Can also be used if you are waiting for a tab to open, but only if you
  // have specified a valid Browser or ElementContext.
  void LoadPageInNewTab(const GURL& url, bool activate_tab);

  // Direct Javascript Evaluation //////////////////////////////////////////////

  // Executes `function` in the target WebContents. Fails if the current page is
  // not loaded or if the script generates an error.
  //
  // Function should be an unnamed javascript function, e.g.:
  //   function() { document.querySelector('#my-button').click(); }
  // or:
  //   () => window.valueToCheck
  //
  // Returns the return value of the function, which may be empty if there is no
  // return value. If the return value is a promise, will block until the
  // promise resolves and then return the result.
  //
  // If you wish to do a background or asynchronous task but not block, have
  // your script return immediately and then call SendEventOnStateChange() to
  // monitor the result.
  base::Value Evaluate(const std::string& function);

  // Watches for a state change in the current page, then sends an event when
  // the condition is met or (optionally) if the timeout is hit. The page must
  // be fully loaded.
  //
  // Unlike calling Evaluate() and returning a promise, this code does not
  // block; you will receive a callback on the main thread when the condition
  // changes.
  //
  // If a page navigates away or closes before the state change happens or the
  // timeout is hit, an error is generated.
  void SendEventOnStateChange(const StateChange& configuration);

  // DOM and Shadow DOM Manipulation ///////////////////////////////////////////

  // Returns true if there is an element at `query`, false otherwise. If
  // `not_found` is not null, it will receive the value of the element not
  // found, or an empty string if the function returns true.
  bool Exists(const DeepQuery& query, std::string* not_found = nullptr);

  // Evaluates `function` on the element returned by finding the element at
  // `where`; throw an error if `where` doesn't exist or capture with a second
  // argument.
  // The `function` parameter should be the text of a valid javascript unnamed
  // function that takes a DOM element and/or an error parameter if occurs and
  // optionally returns a value.
  //
  // Example:
  //   function(el) { return el.innterText; }
  // Or capture the error instead of throw:
  //   (el, err) => !err && !!el
  base::Value EvaluateAt(const DeepQuery& where, const std::string& function);

  // The following are convenience methods that do not use the Shadow DOM and
  // allow only a single selector (behavior if the selected node has a shadow
  // DOM is undefined).
  bool Exists(const std::string& selector);
  base::Value EvaluateAt(const std::string& where, const std::string& function);

 protected:
  // content::WebContentsObserver:
  void DidStopLoading() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(InteractionSequenceBrowserUtilInteractiveUiTest,
                           OpenTabSearchMenuAndTestVisibility);
  class NewTabWatcher;
  class Poller;
  struct PollerData;
  class WebViewData;

  InteractionSequenceBrowserUtil(content::WebContents* web_contents,
                                 ui::ElementIdentifier page_identifier,
                                 absl::optional<Browser*> browser,
                                 views::WebView* web_view);

  void MaybeCreateElement(bool force = false);
  void DiscardCurrentElement();

  void OnPollTimeout(Poller* poller);
  void OnPollEvent(Poller* poller);

  void StartWatchingWebContents(content::WebContents* web_contents);

  // Dictates the identifier that will be assigned to the new
  // TrackedElementWebPage created for the target WebContents on the next page
  // load.
  ui::ElementIdentifier page_identifier_;

  // When we force a page load, we might still get events for the old page.
  // We'll ignore those events.
  absl::optional<GURL> navigating_away_from_;

  // Tracks the WebView that hosts a non-tab WebContents; null otherwise.
  std::unique_ptr<WebViewData> web_view_data_;

  // Virtual element representing the currently-loaded webpage; null if none.
  std::unique_ptr<TrackedElementWebPage> current_element_;

  // List of active event pollers for the current page.
  std::map<Poller*, PollerData> pollers_;

  // Optional object that watches for a new tab to be created, either in a
  // specific browser or in any browser.
  std::unique_ptr<NewTabWatcher> new_tab_watcher_;
};

// Represents a loaded web page. Created and shown by
// InteractionSequenceBrowserUtil when the WebContents it is watching fully
// loads a page and then hidden and destroyed when the page unloads, navigates
// away, or is closed.
class TrackedElementWebPage : public ui::TrackedElement {
 public:
  TrackedElementWebPage(ui::ElementIdentifier identifier,
                        ui::ElementContext context,
                        InteractionSequenceBrowserUtil* owner);
  ~TrackedElementWebPage() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  InteractionSequenceBrowserUtil* owner() { return owner_; }

 private:
  friend InteractionSequenceBrowserUtil;

  void Init();

  const base::raw_ptr<InteractionSequenceBrowserUtil> owner_;
};

#endif  // CHROME_TEST_INTERACTION_INTERACTION_SEQUENCE_BROWSER_UTIL_H_
