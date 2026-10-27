// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_

#include <memory>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

class AvatarToolbarButton;
class BrowserWindowInterface;
class WebUIAvatarToolbarButton;
enum class AvatarToolbarButtonState;
class WebUIToolbarWebView;

namespace ui {
class ElementIdentifier;
class TrackedElement;
}  // namespace ui

namespace base {
class FilePath;
}  // namespace base

namespace content {
struct DropData;
class WebContents;
}  // namespace content

namespace views {
class View;
class WebView;
class Widget;
}  // namespace views

// Waits until the initial WebUI component has performed its first non-empty
// paint.
void WaitUntilInitialWebUIPaintAndFlushMetricsForTesting(
    BrowserWindowInterface* browser);

// Waits until the InitialWebUIManager says the toolbar is ready.
void WaitForInitialWebUIToolbar(BrowserWindowInterface* browser);

// Sets up the WebUI toolbar for testing by waiting for the toolbar view,
// resolving the element associated with `element_id`, extracting the child
// WebView, and waiting for it to finish composition.
void SetUpWebUI(const ui::ElementIdentifier& element_id,
                ui::TrackedElement** element_out,
                WebUIToolbarWebView** webui_toolbar_view_out,
                views::WebView** web_view_out,
                BrowserWindowInterface* browser);

// Retrieves the WebUIToolbarWebView instance associated with the given
// `browser`.
WebUIToolbarWebView* GetWebUIToolbarWebView(BrowserWindowInterface* browser);

// Simulates a left-click on the WebUI toolbar extension button with the given
// `id` (or the puzzle piece extensions menu button if `id` is empty).
void LeftClickExtensionButton(content::WebContents* web_contents,
                              const std::string& id);

// Simulates a right-click (context menu) on the WebUI toolbar extension button
// with the given `id`.
void RightClickExtensionButton(content::WebContents* web_contents,
                               const std::string& id);

// Returns JavaScript expression selecting an element inside toolbar-app.
std::string GetButtonAppJS(const std::string& selector);

// Checks if an element matching `selector` inside toolbar-app is visible.
bool IsButtonVisible(content::WebContents* web_contents,
                     const std::string& selector);

// Waits until an element matching `selector` inside toolbar-app is visible.
bool WaitForButtonVisible(content::WebContents* web_contents,
                          const std::string& selector);

// Waits until an element matching `selector` inside toolbar-app is hidden.
bool WaitForButtonHidden(content::WebContents* web_contents,
                         const std::string& selector);

// Pins a button preference and waits for composition in WebUI toolbar.
void PinButton(BrowserWindowInterface* browser,
               views::WebView* web_view,
               const char* pref);

// CSS selector for the WebUI toolbar home button.
inline constexpr char kHomeSelector[] = "#home";

// Pins Home button and waits for it to become visible in WebUI toolbar.
WebUIToolbarWebView* SetUpAndPinHomeButton(BrowserWindowInterface* browser);

// Returns JS expression selecting the inner icon/chip button element.
std::string GetButtonIconJS(const std::string& selector);

// JavaScript snippet that computes center x and y coordinates of `target`.
extern const char kGetCoordinatesJS[];

// Adds functions to `target` to mimic pointer capture functions. Note that real
// pointer capture is lost on pointer up, but the returned functions cannot
// handle that, so if that is important for a test, it must manually call
// `releasePointerCapture('*')`.
std::string AddMockPointerCaptureFunctions(const char* target);

// Maximum number of animation frames (~800ms at 60Hz) to wait for asynchronous
// Mojo state changes and Lit element render cycles to settle before timing out.
// Tests typically finish within 1-2 frames; this constant serves as an upper
// bound to fail quickly rather than hanging until the browser test timeout.
inline constexpr int kMaxWebUIWaitFrames = 50;

// JavaScript snippet defining `findDeep(root, selectorOrPredicate)` to search
// through open Shadow DOM trees to find an element matching either a CSS
// selector string or a predicate function.
extern const char kFindDeepJS[];

// Fast-forwards and completes all active animations and CSS transitions across
// all open ShadowRoot subtrees in `web_contents`.
// If `custom_wait_js` is non-empty, it is executed first inside the async
// runner before animations are gathered and finished. Note that
// `findDeep(root, selectorOrPredicate)` is automatically defined in the script
// context and is available to `custom_wait_js`.
// Returns AssertionSuccess if the execution succeeds, or AssertionFailure with
// the failure message if it fails.
[[nodiscard]] ::testing::AssertionResult FinishWebUIAnimations(
    content::WebContents* web_contents,
    std::string_view custom_wait_js = "");

// Makes the browser and the WebUI toolbar of `browser` agree on everything
// that has happened so far. Returns a failure if there is no bound WebUI
// toolbar.
//
// The toolbar talks to the browser over its own Mojo pipes (ToolbarUIObserver
// for browser->WebUI state pushes, ToolbarUIService for WebUI->browser
// actions), which are not ordered against the frame IPC that test waiters
// usually key off (focus-change notifications, key events, ...). A test that
// resumes on such a signal and then relies on state carried by the toolbar
// pipes must call this first.
//
// This sends any pending coalesced state push, round-trips the observer pipe
// (the WebUI has applied every push and sent its reactions), then round-trips
// the service pipe (the browser has handled those reactions).
[[nodiscard]] ::testing::AssertionResult FlushWebUIToolbarPipes(
    BrowserWindowInterface* browser);

// Sets the window during which WebUI toolbar buttons ignore a click that
// follows the closing of a bubble anchored to them (which would otherwise
// reopen the bubble, see WebUIBubbleReopenSuppressor and HighlightTracker in
// toolbar_button.ts). Covers both the native-side suppressors owned by the
// WebUI location bar and the WebUI-side HighlightTracker. Pass
// base::TimeDelta() to disable the post-close time window, e.g. for tests that
// close a bubble (by switching tabs, say) and immediately click its anchor
// again. Clicks that land while the bubble is still showing are suppressed
// regardless of `threshold`. Waits for the toolbar WebUI to finish loading.
// Returns a failure if there is no WebUI toolbar.
[[nodiscard]] ::testing::AssertionResult
SetWebUIToolbarClickSuppressionThreshold(BrowserWindowInterface* browser,
                                         base::TimeDelta threshold);

// Dispatches an event to a WebUI toolbar button.
// `selector`: The CSS selector for the button element.
// `event_class`: The JS event class (e.g. 'MouseEvent', 'PointerEvent').
// `type`: The event type string (e.g. 'click', 'contextmenu').
// `options`: JS object string for event options (e.g. "detail: 1, button: 2").
std::string DispatchEventScript(const std::string& selector,
                                const std::string& event_class,
                                const std::string& type,
                                const std::string& options = "");

// Simulates a physical pointer press + release cycle without dispatching a
// top-level 'click' event. This is practically sufficient for interacting with
// some custom WebUI elements (like CrIconButton) that bind their action logic
// exclusively to 'pointerup'. It is also strictly required for simulating
// auxiliary interactions (like middle-clicks) which do not produce standard
// 'click' events.
std::string DispatchPointerDownAndUp(
    const std::string& selector,
    const std::string& pointer_type = "mouse",
    const std::string& opts = "detail: 1, button: 0");

// Counts navigations started in a WebContents, and can assert that none
// happened.
class NavigationCounter : public content::WebContentsObserver {
 public:
  explicit NavigationCounter(content::WebContents* web_contents);
  ~NavigationCounter() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // A helper that waits some time and then checks that no navigations occurred.
  void WaitForNoNavigations();

  size_t navigation_count() const { return navigation_count_; }

 private:
  size_t navigation_count_ = 0;
};

// Drops data on the WebUI toolbar through the same sequence of calls that
// `WebContentsViewAura` makes for a real drag, so that both the drag
// provenance tracked by `PreHandleDragUpdate` and the data the WebUI receives
// match production.
//
// Shared by the drop tests of the individual toolbar controls, which only
// differ in where the drop lands and in what they expect to happen.
class WebUIToolbarDropSimulator {
 public:
  enum class DragOrigin {
    // Dragged out of a web page rendered by a renderer process.
    kWebPage,
    // Dragged in from outside the browser (a text editor, a file manager...).
    kOs,
  };

  // `toolbar_view` anchors the client coordinates of the drop point in screen
  // space. `toolbar_web_contents` hosts the WebUI toolbar receiving the drop.
  // Both must outlive this object.
  WebUIToolbarDropSimulator(views::View* toolbar_view,
                            content::WebContents* toolbar_web_contents);
  WebUIToolbarDropSimulator(const WebUIToolbarDropSimulator&) = delete;
  WebUIToolbarDropSimulator& operator=(const WebUIToolbarDropSimulator&) =
      delete;
  ~WebUIToolbarDropSimulator();

  // Sets where drops land, in the toolbar WebContents' client coordinates.
  void set_drop_point(const gfx::PointF& drop_point) {
    drop_point_ = drop_point;
  }
  const gfx::PointF& drop_point() const { return drop_point_; }

  // Runs a full drag-enter, drag-over, drop sequence at the drop point.
  // Returns whether the toolbar accepted the drag. A refused drag receives no
  // drop, as the platform does not deliver one after the target rejects every
  // drag operation.
  //
  // When `drag_exited_before_drop` is true, the toolbar is told that the drag
  // left it after the last drag update, as happens when the drag leaves the
  // toolbar area before the drop lands.
  bool PerformDrop(content::DropData drop_data,
                   bool drag_exited_before_drop = false);

  // Drop of a link, i.e. `text/uri-list` only. Expects the drag to be
  // accepted.
  void SimulateLinkDrop(const std::string& url, DragOrigin origin);

  // Drop of selected text, i.e. `text/plain`. The drop data is built through
  // `ui::OSExchangeData`, matching `WebContentsViewAura::PrepareDropData`, so
  // platforms that synthesize a URL from URL-like plain text (Windows,
  // ChromeOS, Wayland, macOS) also add `text/uri-list`, while X11 does not.
  // Expects the drag to be accepted. Returns whether a URL was synthesized.
  bool SimulateTextDrop(const std::string& text, DragOrigin origin);

  // Drop carrying `url` in both `text/uri-list` and `text/plain`. Expects the
  // drag to be accepted.
  void SimulateLinkWithTextDrop(const std::string& url, DragOrigin origin);

  // Drop of an OS file, i.e. `Files` plus `DropData::filenames`. Returns
  // whether the toolbar accepted the drag.
  bool SimulateFileDrop(const base::FilePath& path,
                        DragOrigin origin,
                        bool drag_exited_before_drop = false);

 private:
  raw_ptr<views::View> toolbar_view_;
  raw_ptr<content::WebContents> toolbar_web_contents_;
  gfx::PointF drop_point_;
};

class AvatarButtonUpdateWaiter : public AvatarToolbarButtonInterface::Observer {
 public:
  explicit AvatarButtonUpdateWaiter(AvatarToolbarButtonInterface* button);
  ~AvatarButtonUpdateWaiter() override;

  AvatarButtonUpdateWaiter(const AvatarButtonUpdateWaiter&) = delete;
  AvatarButtonUpdateWaiter& operator=(const AvatarButtonUpdateWaiter&) = delete;

  void Wait();

  // AvatarToolbarButtonInterface::Observer:
  void OnIconUpdated() override;

 private:
  bool updated_ = false;
  base::RunLoop run_loop_;
  base::RepeatingClosure quit_closure_;
  base::ScopedObservation<AvatarToolbarButtonInterface,
                          AvatarToolbarButtonInterface::Observer>
      scoped_observation_{this};
};

struct AvatarLayoutAttributes {
  int icon_width = 0;
  int icon_height = 0;
  gfx::Insets insets;
  bool has_linear_gradient_ring = false;
};

class AvatarToolbarButtonTestAccessor {
 public:
  using ButtonVariant = std::variant<raw_ptr<AvatarToolbarButton>,
                                     raw_ptr<WebUIAvatarToolbarButton>>;

  explicit AvatarToolbarButtonTestAccessor(BrowserWindowInterface* browser);
  ~AvatarToolbarButtonTestAccessor();
  void WaitForAvatarButton();
  bool WaitForText(const std::u16string& text);
  bool WaitForTextNotEqual(const std::u16string& text);
  bool WaitForState(AvatarToolbarButtonState state);
  std::unique_ptr<AvatarButtonUpdateWaiter> CreateUpdateWaiter();
  AvatarToolbarButtonState GetState();
  bool WaitForRenderedTooltipText(const std::u16string& text);
  bool WaitForAccessibilityLabel(const std::u16string& text);
  bool WaitForAccessibilityDescription(const std::u16string& text);
  bool WaitForEnabled(bool enabled);
  bool GetEnabled();
  bool GetVisible();
  std::u16string GetText();
  views::Widget* GetWidget();
  gfx::ImageSkia GetImage(views::Button::ButtonState state);
  std::string GetImageUrl();
  std::u16string GetRenderedTooltipText(const gfx::Point& p);
  std::u16string GetAccessibilityLabel();
  std::u16string GetAccessibilityDescription();
  AvatarLayoutAttributes GetLayoutAttributes();
  bool WaitForLinearGradientRing(bool has_ring);
  void Click();
  void SetAnnounceCallbackForTesting(
      base::OnceCallback<void(std::u16string)> callback);

 private:
  content::WebContents* GetWebContents();
  AvatarToolbarButtonInterface* GetInterface();
  bool ShouldUseCppFallback(WebUIAvatarToolbarButton* button);
  ButtonVariant GetButton();

  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
