// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_WEBCONTENTS_INTERACTION_TEST_UTIL_H_
#define CHROME_TEST_INTERACTION_WEBCONTENTS_INTERACTION_TEST_UTIL_H_

#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace views {
class WebView;
}

class Browser;

class TrackedElementWebContents;

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
class WebContentsInteractionTestUtil : private content::WebContentsObserver,
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
  //
  // Best practice is to use the smallest number of `segments` and the fewest
  // terms within each segment.
  //
  // For example, rather than:
  //   {
  //     "my-app", "#container", ".body-list:nth-child(2)", "sub-component",
  //     "div", "span", "#target"
  //   }
  //
  // Prefer:
  //   { "my-app", ".body-list:nth-child(2) sub-component", "#target" }
  //
  // More concise queries are easier to read and less fragile if the structure
  // of the underlying page changes.
  class DeepQuery {
   public:
    DeepQuery();
    DeepQuery(std::initializer_list<std::string> segments);
    DeepQuery(const DeepQuery& other);
    DeepQuery& operator=(const DeepQuery& other);
    DeepQuery& operator=(std::initializer_list<std::string> segments);
    DeepQuery operator+(const std::string& segment) const;
    ~DeepQuery();

    using const_iterator = std::vector<std::string>::const_iterator;
    using size_type = std::vector<std::string>::size_type;

    const_iterator begin() const { return segments_.begin(); }
    const_iterator end() const { return segments_.end(); }

    bool empty() const { return segments_.empty(); }
    size_type size() const { return segments_.size(); }
    const std::string& operator[](size_type which) const {
      return segments_[which];
    }

   private:
    friend void PrintTo(
        const WebContentsInteractionTestUtil::DeepQuery& deep_query,
        std::ostream* os);

    std::vector<std::string> segments_;
  };

  // Specifies a state change in a web page that we would like to poll for.
  // By using `event` and `timeout_event` you can determine both that an
  // expected state change happens in the expected amount of time, or that a
  // state change *doesn't* happen in a particular length of time.
  struct StateChange {
    StateChange();
    StateChange(const StateChange& other);
    StateChange& operator=(const StateChange& other);
    ~StateChange();

    // What type of state change are we watching for?
    enum class Type {
      // Automatically chooses one of the other types, based on which of
      // `test_function` and `where` are set. Will never choose `kDoesNotExist`
      // (default).
      kAuto,
      // Triggers when `test_function` returns true. The `where` field
      // should not be set.
      kConditionTrue,
      // Triggers when the element specified by `where` exists in the DOM.
      // The `test_function` field should not be set.
      kExists,
      // Triggers when the element specified by `where` exists in the DOM *and*
      // `test_function` evaluates to true. Both must be set.
      kExistsAndConditionTrue,
      // Triggers if/when the element specified by `where` no longer exists.
      // The `test_function` field should not be set.
      kDoesNotExist
    };

    // By default the type of state change is inferred from the other
    // parameters. This may be set explicitly, but it should only be required
    // for `kDoesNotExist` as there is no way to infer that option.
    Type type = Type::kAuto;

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
    std::optional<base::TimeDelta> timeout;

    // If this is set to `true`, the condition will continue to be polled across
    // page navigation. This can be used when the target WebContents may
    // transition through one or more intermediate pages before the expected
    // condition is met.
    bool continue_across_navigation = false;

    // The event to fire when `test_script` returns a truthy value. Must be
    // specified.
    ui::CustomElementEventType event;

    // The event to fire if `timeout` is hit before `test_script` returns a
    // truthy value. If not specified, generates an error on timeout.
    ui::CustomElementEventType timeout_event;
  };

  ~WebContentsInteractionTestUtil() override;

  // Creates an object associated with a WebContents in the Browser associated
  // with `context`. The TrackedElementWebContents associated with loaded pages
  // will be created with identifier `page_identifier` but you can later change
  // this by calling set_page_identifier(). If `tab_index` is specified, a
  // particular tab will be used, but if it is not, the active tab is used
  // instead.
  static std::unique_ptr<WebContentsInteractionTestUtil>
  ForExistingTabInContext(ui::ElementContext context,
                          ui::ElementIdentifier page_identifier,
                          std::optional<int> tab_index = std::nullopt);

  // As above, but you may directly specify the Browser to use.
  static std::unique_ptr<WebContentsInteractionTestUtil>
  ForExistingTabInBrowser(Browser* browser,
                          ui::ElementIdentifier page_identifier,
                          std::optional<int> tab_index = std::nullopt);

  // Creates a util object associated with a WebContents, which must be in a
  // tab. The associated TrackedElementWebContents will be assigned
  // `page_identifier`.
  static std::unique_ptr<WebContentsInteractionTestUtil> ForTabWebContents(
      content::WebContents* web_contents,
      ui::ElementIdentifier page_identifier);

  // Creates a util object associated with a WebView in a secondary UI (e.g. the
  // touch tabstrip, tab search box, side panel, etc.) The associated
  // TrackedElementWebContents will be assigned `page_identifier`.
  static std::unique_ptr<WebContentsInteractionTestUtil> ForNonTabWebView(
      views::WebView* web_view,
      ui::ElementIdentifier page_identifier);

  // Creates a util object that becomes valid (and creates an element with
  // identifier `page_identifier`) when the next tab is created in the Browser
  // associated with `context` and references that new WebContents.
  static std::unique_ptr<WebContentsInteractionTestUtil> ForNextTabInContext(
      ui::ElementContext context,
      ui::ElementIdentifier page_identifier);

  // Creates a util object that becomes valid (and creates an element with
  // identifier `page_identifier`) when the next tab is created in `browser`
  // and references that new WebContents.
  static std::unique_ptr<WebContentsInteractionTestUtil> ForNextTabInBrowser(
      Browser* browser,
      ui::ElementIdentifier page_identifier);

  // Creates a util object that becomes valid (and creates an element with
  // identifier `page_identifier`) when the next tab is created in any browser
  // and references the new WebContents.
  static std::unique_ptr<WebContentsInteractionTestUtil> ForNextTabInAnyBrowser(
      ui::ElementIdentifier page_identifier);

  // Returns whether the given value is "truthy" in the Javascript sense.
  static bool IsTruthy(const base::Value& value);

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

  // Returns whether any non-empty paints have occurred on the current page.
  // The first non-empty paint is required before a page can be screenshot or
  // receive keyboard or accelerator input. Note that completely empty pages
  // will not receive a paint.
  bool HasPageBeenPainted() const;

  // Returns the instrumented WebView, or null if none.
  views::WebView* GetWebView();

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
  // If `error_msg` is specified, receives an error message on an uncaught
  // exception, and the return value will be `NONE`. If `error_msg` is not
  // specified, crashes on failure.
  //
  // If you wish to do a background or asynchronous task but not block, have
  // your script return immediately and then call SendEventOnStateChange() to
  // monitor the result.
  base::Value Evaluate(const std::string& function,
                       std::string* error_msg = nullptr);

  // Executes `function` in the target WebContents. Identical to `Evaluate()`
  // except that the return value of the function is discarded and no effort is
  // made to wait for the code to actually execute.
  //
  // Execute can be more efficient than Evaluate because it does not hold the
  // test fixture up waiting for completion; the trade-off is that if there is
  // an error during execution it will not immediately crash the test (though it
  // should still be visible in the logs).
  void Execute(const std::string& function);

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
  //
  // The `function` parameter should be the text of a valid javascript unnamed
  // function that takes a DOM element and/or an error parameter if occurs and
  // optionally returns a value.
  //
  // If `error_msg` is specified, receives an error message on an uncaught
  // exception, and the return value will be `NONE`. If `error_msg` is not
  // specified, crashes on failure.
  //
  // Example:
  //   function(el) { return el.innterText; }
  // Or capture the error instead of throw:
  //   (el, err) => !err && !!el
  base::Value EvaluateAt(const DeepQuery& where,
                         const std::string& function,
                         std::string* error_message = nullptr);

  // Same as EvaluateAt except that `function` is executed, the return value is
  // discarded, and no effort is made to wait for or return the result.
  //
  // ExecuteAt can be more efficient than Evaluate because it does not hold the
  // test fixture up waiting for completion; the trade-off is that if there is
  // an error during execution it will not immediately crash the test (though it
  // should still be visible in the logs).
  void ExecuteAt(const DeepQuery& where, const std::string& function);

  // The following are convenience methods that do not use the Shadow DOM and
  // allow only a single selector (behavior if the selected node has a shadow
  // DOM is undefined).
  bool Exists(const std::string& selector);
  base::Value EvaluateAt(const std::string& where, const std::string& function);
  void ExecuteAt(const std::string& where, const std::string& function);

  // Gets the screen bounds for the given element at `where`. The second method
  // is a convenience method if you do not need to use the Shadow DOM.
  //
  // Note that the result is in DIPs and *may* be inaccurate if the screen's
  // scale factor is not 100% (see discussion in implementation).
  //
  // If the element is in a tab or window that is not visible, an empty `Rect`
  // will be returned.
  gfx::Rect GetElementBoundsInScreen(const DeepQuery& where);
  gfx::Rect GetElementBoundsInScreen(const std::string& where);

  // Miscellaneous Tools ///////////////////////////////////////////////////////

 protected:
  // content::WebContentsObserver:
  void DidStopLoading() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;
  void DidFirstVisuallyNonEmptyPaint() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebContentsInteractionTestUtilInteractiveUiTest,
                           OpenTabSearchMenuAndTestVisibility);
  class NewTabWatcher;
  class Poller;
  class WebViewData;

  WebContentsInteractionTestUtil(content::WebContents* web_contents,
                                 ui::ElementIdentifier page_identifier,
                                 std::optional<Browser*> browser,
                                 views::WebView* web_view);

  void MaybeCreateElement();
  void MaybeSendPaintEvent();
  void DiscardCurrentElement();

  void OnPollEvent(Poller* poller, ui::CustomElementEventType event);

  void StartWatchingWebContents(content::WebContents* web_contents);

  // Dictates the identifier that will be assigned to the new
  // TrackedElementWebContents created for the target WebContents on the next
  // page load.
  ui::ElementIdentifier page_identifier_;

  // When we force a page load, we might still get events for the old page.
  // We'll ignore those events.
  std::optional<GURL> navigating_away_from_;

  // Whether a painted event was sent for the current page.
  bool sent_paint_event_ = false;

  // Tracks the WebView that hosts a non-tab WebContents; null otherwise.
  std::unique_ptr<WebViewData> web_view_data_;

  // Virtual element representing the currently-loaded webpage; null if none.
  std::unique_ptr<TrackedElementWebContents> current_element_;

  // List of active event pollers for the current page.
  std::list<std::unique_ptr<Poller>> pollers_;

  // Optional object that watches for a new tab to be created, either in a
  // specific browser or in any browser.
  std::unique_ptr<NewTabWatcher> new_tab_watcher_;
};

extern void PrintTo(const WebContentsInteractionTestUtil::DeepQuery& deep_query,
                    std::ostream* os);

extern std::ostream& operator<<(
    std::ostream& os,
    const WebContentsInteractionTestUtil::DeepQuery& deep_query);

extern void PrintTo(
    const WebContentsInteractionTestUtil::StateChange& state_change,
    std::ostream* os);

extern std::ostream& operator<<(
    std::ostream& os,
    const WebContentsInteractionTestUtil::StateChange& state_change);

#endif  // CHROME_TEST_INTERACTION_WEBCONTENTS_INTERACTION_TEST_UTIL_H_
