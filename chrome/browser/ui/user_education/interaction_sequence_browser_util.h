// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_INTERACTION_SEQUENCE_BROWSER_UTIL_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_INTERACTION_SEQUENCE_BROWSER_UTIL_H_

#include <map>

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
//
// The TrackedElementWebContents created to represent each page becomes
// "visible" when the main frame finishes loading, and is "hidden" when a new
// navigation commits or the WebContents is closed or moved out of the browser.
class InteractionSequenceBrowserUtil : private content::WebContentsObserver,
                                       private TabStripModelObserver {
 public:
  // How often to poll for state changes we're watching; see
  // SendEventOnStateChange().
  static constexpr base::TimeDelta kDefaultPollingInterval =
      base::Milliseconds(200);

  // Specifies a state change in a web page that we would like to poll for.
  // By using `event` and `timeout_event` you can determine both that an
  // expected state change happens in the expected amount of time, or that a
  // state change *doesn't* happen in a particular length of time.
  struct StateChange {
    // Script to be evaluated every `polling_interval`. Must be able to execute
    // multiple times successfully. State change is detected when this script
    // returns a "truthy" value.
    std::string test_script;

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

  // Creates an object associated with a WebContents in the Browser associated
  // with `context`. The TrackedElementWebContents associated with loaded pages
  // will be created with identifier `page_identifier` but you can later change
  // this by calling set_page_identifier(). If `tab_index` is specified, a
  // particular tab will be used, but if it is not, the active tab is used
  // instead.
  InteractionSequenceBrowserUtil(ui::ElementContext context,
                                 ui::ElementIdentifier page_identifier,
                                 absl::optional<int> tab_index = absl::nullopt);

  // As above, but you may directly specify the Browser to use.
  InteractionSequenceBrowserUtil(Browser* browser,
                                 ui::ElementIdentifier page_identifier,
                                 absl::optional<int> tab_index = absl::nullopt);

  // As above, but with a specific WebContents.
  InteractionSequenceBrowserUtil(content::WebContents* web_contents,
                                 ui::ElementIdentifier page_identifier);

  ~InteractionSequenceBrowserUtil() override;

  // Returns the browser that matches the given context, or nullptr if none
  // can be found.
  static Browser* GetBrowserFromContext(ui::ElementContext context);

  // Returns whether the given value is "truthy" in the Javascript sense.
  static bool IsTruthy(const base::Value& value);

  // Loads a page in the target WebContents. Uses the renderer directly instead
  // of simulating a Browser navigation and does not block; you can wait for
  // the subsequent kShown event, etc. to determine when the page is actually
  // loaded. The command must succeed or an error will be generated.
  void LoadPage(const GURL& url);

  // Executes `script` in the target WebContents. Fails if the current page is
  // not loaded or if the script generates an
  // error. Returns the result of the script, which may be empty if there is
  // no return value.
  base::Value Evaluate(const std::string& script);

  // Watches for a state change event in the current page.
  //
  // Cannot be called when is_page_loaded() is false; you can also watch for
  // the "shown" event associated with page_identifier() to know when it is
  // safe to register for events.
  void SendEventOnStateChange(const StateChange& configuration);

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
  class Poller;
  struct PollerData;

  void MaybeCreateElement(bool force = false);
  void DiscardCurrentElement();

  void OnPollTimeout(Poller* poller);
  void OnPollEvent(Poller* poller);

  // Dictates the identifier that will be assigned to the new
  // TrackedElementWebPage created for the target WebContents on the next page
  // load.
  ui::ElementIdentifier page_identifier_;

  // Virtual element representing the currently-loaded webpage; null if none.
  std::unique_ptr<TrackedElementWebPage> current_element_;

  // List of active event pollers for the current page.
  std::map<Poller*, PollerData> pollers_;
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

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_INTERACTION_SEQUENCE_BROWSER_UTIL_H_
