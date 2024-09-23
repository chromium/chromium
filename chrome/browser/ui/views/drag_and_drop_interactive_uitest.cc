// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace chrome {
namespace {

using ::ui::mojom::DragOperation;

// TODO(lukasza): Support testing on non-Aura platforms (i.e. Android + Mac?).
//
// Notes for the TODO above:
//
// - Why inject/simulate drag-and-drop events at the aura::Window* level.
//
//   - It seems better to inject into UI libraries to cover code *inside* these
//     libraries.  This might complicate simulation a little bit (i.e. picking
//     the right aura::Window and/or aura::client::DragDropDelegate to target),
//     but otherwise important bits of code wouldn't get test coverage (i.e.
//     directly injecting into RenderViewHost->DragTargetDragEnter seems wrong).
//
//   - In theory, we could introduce WebContentsImpl::DragTargetDragEnter (to be
//     used by all UI platforms - so reused by web_contents_view_android.cc,
//     web_contents_view_aura.cc, web_drag_dest_mac.mm), but it feels wrong - UI
//     libraries should already know which widget is the target of the event and
//     so should be able to talk directly to the right widget (i.e. WebContents
//     should not be responsible for mapping coordinates to a widget - this is
//     the job of the UI library).
//
// - Unknowns:
//
//   - Will this work for WebView and Plugin testing.

// Test helper for simulating drag and drop happening in WebContents.
class DragAndDropSimulator {
 public:
  explicit DragAndDropSimulator(content::WebContents* web_contents)
      : DragAndDropSimulator(web_contents, web_contents) {}

  DragAndDropSimulator(content::WebContents* drag_contents,
                       content::WebContents* drop_contents)
      : drag_contents_(drag_contents), drop_contents_(drop_contents) {}

  DragAndDropSimulator(const DragAndDropSimulator&) = delete;
  DragAndDropSimulator& operator=(const DragAndDropSimulator&) = delete;

  // Simulates notification that |text| was dragged from outside of the browser,
  // into the specified |location| inside |web_contents|.
  // |location| is relative to |web_contents|.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location, const std::string& text) {
    os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
    os_exchange_data_->SetString(base::UTF8ToUTF16(text));
    return SimulateDragEnter(location, *os_exchange_data_);
  }

  // Simulates notification that |url| was dragged from outside of the browser,
  // into the specified |location| inside |web_contents|.
  // |location| is relative to |web_contents|.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location, const GURL& url) {
    os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
    os_exchange_data_->SetURL(url, base::UTF8ToUTF16(url.spec()));
    return SimulateDragEnter(location, *os_exchange_data_);
  }

  // Simulates notification that |file| was dragged from outside of the browser,
  // into the specified |location| inside |web_contents|.
  // |location| is relative to |web_contents|.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location,
                         const base::FilePath& file) {
    os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
    os_exchange_data_->SetFilename(file);
    return SimulateDragEnter(location, *os_exchange_data_);
  }

  // Simulates notification that |url| was dragged from outside of the browser,
  // into the specified |location| inside |omnibox|.
  // |location| is relative to |omnibox|.
  // Returns true upon success.
  bool SimulateOmniboxDragEnter(aura::Window* omnibox,
                                const gfx::Point& location,
                                const GURL& url) {
    os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
    os_exchange_data_->SetURL(url, base::UTF8ToUTF16(url.spec()));
    if (active_drag_event_) {
      ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
      return false;
    }

    aura::client::DragDropDelegate* delegate =
        GetOmniboxDragDropDelegate(omnibox);
    if (!delegate) {
      return false;
    }

    active_drag_event_ = base::WrapUnique(new ui::DropTargetEvent(
        *os_exchange_data_, gfx::PointF(location), gfx::PointF(location),
        kDefaultSourceOperations));

    delegate->OnDragEntered(*active_drag_event_);
    delegate->OnDragUpdated(*active_drag_event_);
    return true;
  }

  // Simulates dropping of the drag-and-dropped item.
  // SimulateDragEnter needs to be called first.
  // Returns true upon success.
  bool SimulateDrop(const gfx::Point& location) {
    if (!active_drag_event_) {
      ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
      return false;
    }

    aura::client::DragDropDelegate* delegate = GetDropDelegate();
    if (!delegate) {
      return false;
    }

    gfx::PointF event_location;
    gfx::PointF event_root_location;
    CalculateEventLocations(location, &event_location, &event_root_location,
                            drop_contents_);
    active_drag_event_->set_location_f(event_location);
    active_drag_event_->set_root_location_f(event_root_location);

    delegate->OnDragUpdated(*active_drag_event_);
    auto drop_cb = delegate->GetDropCallback(*active_drag_event_);
    // 'drop_cb' should have a value because WebContentsViewAura
    // (DragDropDelegate) doesn't return NullCallback.
    DCHECK(drop_cb);
    ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(std::move(os_exchange_data_), output_drag_op,
                           /*drag_image_layer_owner=*/nullptr);
    return true;
  }

  // Simulates dropping of the drag-and-dropped item into |omnibox|.
  // SimulateDragEnter needs to be called first.
  // Returns true upon success.
  bool SimulateOmniboxDrop(aura::Window* omnibox, const gfx::Point& location) {
    if (!active_drag_event_) {
      ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
      return false;
    }

    aura::client::DragDropDelegate* delegate =
        GetOmniboxDragDropDelegate(omnibox);
    if (!delegate) {
      return false;
    }

    active_drag_event_->set_location_f(gfx::PointF(location));
    active_drag_event_->set_root_location_f(gfx::PointF(location));

    delegate->OnDragUpdated(*active_drag_event_);
    auto drop_cb = delegate->GetDropCallback(*active_drag_event_);
    ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(std::move(os_exchange_data_), output_drag_op,
                           /*drag_image_layer_owner=*/nullptr);
    return true;
  }

 private:
  bool SimulateDragEnter(const gfx::Point& location,
                         const ui::OSExchangeData& data) {
    if (active_drag_event_) {
      ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
      return false;
    }

    aura::client::DragDropDelegate* delegate = GetDragDelegate();
    if (!delegate) {
      return false;
    }

    gfx::PointF event_location;
    gfx::PointF event_root_location;
    CalculateEventLocations(location, &event_location, &event_root_location,
                            drag_contents_);
    active_drag_event_ = base::WrapUnique(new ui::DropTargetEvent(
        data, event_location, event_root_location, kDefaultSourceOperations));

    delegate->OnDragEntered(*active_drag_event_);
    delegate->OnDragUpdated(*active_drag_event_);
    return true;
  }

  aura::client::DragDropDelegate* GetDragDelegate() {
    gfx::NativeView view = drag_contents_->GetContentNativeView();
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(view);
    EXPECT_TRUE(delegate) << "Expecting WebContents to have DragDropDelegate";
    return delegate;
  }

  aura::client::DragDropDelegate* GetDropDelegate() {
    gfx::NativeView view = drop_contents_->GetContentNativeView();
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(view);
    EXPECT_TRUE(delegate) << "Expecting WebContents to have DragDropDelegate";
    return delegate;
  }

  aura::client::DragDropDelegate* GetOmniboxDragDropDelegate(
      aura::Window* omnibox) {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(omnibox);
    EXPECT_TRUE(delegate) << "Expecting Omnibox to have DragDropDelegate";
    return delegate;
  }

  void CalculateEventLocations(const gfx::Point& web_contents_relative_location,
                               gfx::PointF* out_event_location,
                               gfx::PointF* out_event_root_location,
                               content::WebContents* contents) {
    gfx::NativeView view = contents->GetNativeView();

    *out_event_location = gfx::PointF(web_contents_relative_location);

    gfx::Point root_location = web_contents_relative_location;
    aura::Window::ConvertPointToTarget(view, view->GetRootWindow(),
                                       &root_location);
    *out_event_root_location = gfx::PointF(root_location);
  }

  // These are ui::DropTargetEvent::source_operations_ being sent when manually
  // trying out drag&drop of an image file from Nemo (Ubuntu's file explorer)
  // into a content_shell.
  static constexpr int kDefaultSourceOperations = ui::DragDropTypes::DRAG_MOVE |
                                                  ui::DragDropTypes::DRAG_COPY |
                                                  ui::DragDropTypes::DRAG_LINK;

  // WebContents for where the drag and drop occurs. These can be the same if
  // the drag and drop happens within the same WebContents.
  raw_ptr<content::WebContents> drag_contents_;
  raw_ptr<content::WebContents> drop_contents_;

  std::unique_ptr<ui::DropTargetEvent> active_drag_event_;
  std::unique_ptr<ui::OSExchangeData> os_exchange_data_;
};

// Helper for waiting until a drag-and-drop starts (e.g. in response to a
// mouse-down + mouse-move simulated by the test).
class DragStartWaiter : public aura::client::DragDropClient {
 public:
  // Starts monitoring |web_contents| for a start of a drag-and-drop.
  explicit DragStartWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents),
        message_loop_runner_(new content::MessageLoopRunner),
        suppress_passing_of_start_drag_further_(false),
        drag_started_(false) {
    DCHECK(web_contents_);

    // Intercept calls to the old DragDropClient.
    gfx::NativeWindow root_window =
        web_contents_->GetContentNativeView()->GetRootWindow();
    old_client_ = aura::client::GetDragDropClient(root_window);
    aura::client::SetDragDropClient(root_window, this);
  }

  ~DragStartWaiter() override {
    // Restore the original DragDropClient.
    gfx::NativeWindow root_window =
        web_contents_->GetContentNativeView()->GetRootWindow();
    aura::client::SetDragDropClient(root_window, old_client_);
  }

  DragStartWaiter(const DragStartWaiter&) = delete;
  DragStartWaiter& operator=(const DragStartWaiter&) = delete;

  // Waits until we almost report a drag-and-drop start to the OS.
  // At that point
  // 1) the callback from PostTaskWhenDragStarts will be posted.
  // 2) the drag-start request will be forwarded to the OS
  //    (unless SuppressPassingStartDragFurther method was called).
  //
  // Note that if SuppressPassingStartDragFurther was not called then
  // WaitUntilDragStart can take a long time to return (it returns only after
  // the OS decides that the drag-and-drop has ended).
  //
  // Before returning populates `source_origin`, `text`, `html` and other
  // parameters with data that would have been passed to the OS). If the caller
  // is not interested in this data, then the corresponding argument can be
  // null.
  void WaitUntilDragStart(std::optional<url::Origin>* source_origin,
                          std::string* text,
                          std::string* html,
                          int* operation,
                          gfx::Point* location_inside_web_contents) {
    message_loop_runner_->Run();

    // message_loop_runner_->Quit is only called from StartDragAndDrop.
    DCHECK(drag_started_);

    if (source_origin) {
      *source_origin = source_origin_;
    }
    if (text) {
      *text = text_;
    }
    if (html) {
      *html = html_;
    }
    if (operation) {
      *operation = operation_;
    }
    if (location_inside_web_contents) {
      *location_inside_web_contents = location_inside_web_contents_;
    }
  }

  void WaitUntilDragStart() {
    WaitUntilDragStart(nullptr, nullptr, nullptr, nullptr, nullptr);
  }

  void SuppressPassingStartDragFurther() {
    suppress_passing_of_start_drag_further_ = true;
  }

  void PostTaskWhenDragStarts(base::OnceClosure callback) {
    callback_to_run_inside_drag_and_drop_message_loop_ = std::move(callback);
  }

  // aura::client::DragDropClient overrides:
  DragOperation StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                                 aura::Window* root_window,
                                 aura::Window* source_window,
                                 const gfx::Point& screen_location,
                                 int allowed_operations,
                                 ui::mojom::DragEventSource source) override {
    DCHECK(!drag_started_);
    if (!drag_started_) {
      drag_started_ = true;
      message_loop_runner_->Quit();

      source_origin_ = data->GetRendererTaintedOrigin();
      std::optional<std::u16string> text = data->GetString();
      if (text) {
        text_ = base::UTF16ToUTF8(*text);
      } else {
        text_ = "<no text>";
      }

      std::optional<ui::OSExchangeData::HtmlInfo> html_content =
          data->GetHtml();
      if (html_content.has_value()) {
        html_ = base::UTF16ToUTF8(html_content->html);
      } else {
        html_ = "<no html>";
      }

      gfx::Rect bounds =
          web_contents_->GetContentNativeView()->GetBoundsInScreen();
      location_inside_web_contents_ =
          screen_location - gfx::Vector2d(bounds.x(), bounds.y());

      operation_ = allowed_operations;
    }

    if (!callback_to_run_inside_drag_and_drop_message_loop_.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          std::move(callback_to_run_inside_drag_and_drop_message_loop_));
      callback_to_run_inside_drag_and_drop_message_loop_.Reset();
    }

    if (suppress_passing_of_start_drag_further_) {
      return DragOperation::kNone;
    }

    // Start a nested drag-and-drop loop (might not return for a long time).
    return old_client_->StartDragAndDrop(std::move(data), root_window,
                                         source_window, screen_location,
                                         allowed_operations, source);
  }

  void DragCancel() override {
    ADD_FAILURE() << "Unexpected call to DragCancel";
  }

#if BUILDFLAG(IS_LINUX)
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override {}
#endif

  bool IsDragDropInProgress() override { return drag_started_; }

  void AddObserver(aura::client::DragDropClientObserver* observer) override {}
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override {
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  raw_ptr<aura::client::DragDropClient> old_client_;
  base::OnceClosure callback_to_run_inside_drag_and_drop_message_loop_;
  bool suppress_passing_of_start_drag_further_;

  // Data captured during the first intercepted StartDragAndDrop call.
  bool drag_started_;
  std::optional<url::Origin> source_origin_;
  std::string text_;
  std::string html_;
  int operation_;
  gfx::Point location_inside_web_contents_;
};

// Helper for waiting for notifications from
// content/test/data/drag_and_drop/event_monitoring.js
class DOMDragEventWaiter {
 public:
  DOMDragEventWaiter(const std::string& event_type_to_wait_for,
                     const content::ToRenderFrameHost& target)
      : target_frame_name_(target.render_frame_host()->GetFrameName()),
        event_type_to_wait_for_(event_type_to_wait_for),
        dom_message_queue_(content::WebContents::FromRenderFrameHost(
            target.render_frame_host())) {}

  DOMDragEventWaiter(const DOMDragEventWaiter&) = delete;
  DOMDragEventWaiter& operator=(const DOMDragEventWaiter&) = delete;

  // Waits until |target| calls reportDragEvent in
  // chrome/test/data/drag_and_drop/event_monitoring.js with event_type
  // property set to |event_type_to_wait_for|.  (|target| and
  // |event_type_to_wait_for| are passed to the constructor).
  //
  // Returns the event details via |found_event| (in form of a JSON-encoded
  // object).  See chrome/test/data/drag_and_drop/event_monitoring.js for keys
  // and properties that |found_event| is expected to have.
  //
  // Returns true upon success.  It is okay if |response| is null.
  [[nodiscard]] bool WaitForNextMatchingEvent(std::string* found_event) {
    std::string candidate_event;
    bool got_right_event_type = false;
    bool got_right_window_name = false;
    do {
      if (!dom_message_queue_.WaitForMessage(&candidate_event)) {
        return false;
      }

      got_right_event_type =
          IsExpectedEventType(candidate_event, event_type_to_wait_for_);
      got_right_window_name =
          IsExpectedWindowName(candidate_event, target_frame_name_);
    } while (!got_right_event_type || !got_right_window_name);

    if (found_event) {
      *found_event = candidate_event;
    }

    return true;
  }

  static bool IsExpectedEventType(const std::string& actual_event_body,
                                  const std::string& expected_event_type) {
    return IsExpectedPropertyValue(actual_event_body, "event_type",
                                   expected_event_type);
  }

  static bool IsExpectedWindowName(const std::string& actual_event_body,
                                   const std::string& expected_window_name) {
    return IsExpectedPropertyValue(actual_event_body, "window_name",
                                   expected_window_name);
  }

 private:
  static bool IsExpectedPropertyValue(
      const std::string& actual_event_body,
      const std::string& property_name,
      const std::string& expected_property_value) {
    return base::MatchPattern(
        actual_event_body,
        base::StringPrintf("*\"%s\":\"%s\"*", property_name.c_str(),
                           expected_property_value.c_str()));
  }

  std::string target_frame_name_;
  std::string event_type_to_wait_for_;
  content::DOMMessageQueue dom_message_queue_;
};

// Helper for verifying contents of DOM events associated with drag-and-drop.
class DOMDragEventVerifier {
 public:
  DOMDragEventVerifier() = default;

  DOMDragEventVerifier(const DOMDragEventVerifier&) = delete;
  DOMDragEventVerifier& operator=(const DOMDragEventVerifier&) = delete;

  void set_expected_client_position(const std::string& value) {
    expected_client_position_ = value;
  }

  void set_expected_drop_effect(const std::string& value) {
    expected_drop_effect_ = value;
  }

  void set_expected_effect_allowed(const std::string& value) {
    expected_effect_allowed_ = value;
  }

  void set_expected_file_names(const std::string& value) {
    expected_file_names_ = value;
  }

  void set_expected_mime_types(const std::string& value) {
    expected_mime_types_ = value;
  }

  void set_expected_page_position(const std::string& value) {
    expected_page_position_ = value;
  }

  void set_expected_screen_position(const std::string& value) {
    expected_screen_position_ = value;
  }

  // Returns a matcher that will match a std::string (drag event data - e.g.
  // one returned by DOMDragEventWaiter::WaitForNextMatchingEvent) if it matches
  // the expectations of this DOMDragEventVerifier.
  testing::Matcher<std::string> Matches() const {
    return testing::AllOf(
        FieldMatches("client_position", expected_client_position_),
        FieldMatches("drop_effect", expected_drop_effect_),
        FieldMatches("effect_allowed", expected_effect_allowed_),
        FieldMatches("file_names", expected_file_names_),
        FieldMatches("mime_types", expected_mime_types_),
        FieldMatches("page_position", expected_page_position_),
        FieldMatches("screen_position", expected_screen_position_));
  }

 private:
  static testing::Matcher<std::string> FieldMatches(
      const std::string& field_name,
      const std::string& expected_value) {
    if (expected_value == "<no expectation>") {
      return testing::A<std::string>();
    }

    return testing::HasSubstr(base::StringPrintf(
        "\"%s\":\"%s\"", field_name.c_str(), expected_value.c_str()));
  }

  std::string expected_drop_effect_ = "<no expectation>";
  std::string expected_effect_allowed_ = "<no expectation>";
  std::string expected_file_names_ = "<no expectation>";
  std::string expected_mime_types_ = "<no expectation>";
  std::string expected_client_position_ = "<no expectation>";
  std::string expected_page_position_ = "<no expectation>";
  std::string expected_screen_position_ = "<no expectation>";
};

// Helper for monitoring event notifications from
// content/test/data/drag_and_drop/event_monitoring.js
// and counting how many events of a given type were received.
class DOMDragEventCounter {
 public:
  explicit DOMDragEventCounter(const content::ToRenderFrameHost& target)
      : target_frame_name_(target.render_frame_host()->GetFrameName()),
        dom_message_queue_(content::WebContents::FromRenderFrameHost(
            target.render_frame_host())) {}

  DOMDragEventCounter(const DOMDragEventCounter&) = delete;
  DOMDragEventCounter& operator=(const DOMDragEventCounter&) = delete;

  // Resets all the accumulated event counts to zeros.
  void Reset() {
    StoreAccumulatedEvents();
    received_events_.clear();
  }

  // Returns the number of events of the specified |event_type| received since
  // construction, or since the last time Reset was called.  |event_type| should
  // be one of possible |type| property values for a DOM drag-and-drop event -
  // e.g.  "dragenter" or "dragover".
  int GetNumberOfReceivedEvents(const std::string& event_type) {
    std::vector<std::string> v({event_type});
    return GetNumberOfReceivedEvents(v.begin(), v.end());
  }

  // Returns the number of events of the specified |event_types| received since
  // construction, or since the last time Reset was called.  Elements of
  // |event_types| should be one of possible |type| property values for a DOM
  // drag-and-drop event - e.g.  "dragenter" or "dragover".
  int GetNumberOfReceivedEvents(
      std::initializer_list<const char*> event_types) {
    return GetNumberOfReceivedEvents(event_types.begin(), event_types.end());
  }

 private:
  template <typename T>
  int GetNumberOfReceivedEvents(T event_types_begin, T event_types_end) {
    StoreAccumulatedEvents();

    auto received_event_has_matching_event_type =
        [&event_types_begin,
         &event_types_end](const std::string& received_event) {
          return std::any_of(event_types_begin, event_types_end,
                             [&received_event](const std::string& event_type) {
                               return DOMDragEventWaiter::IsExpectedEventType(
                                   received_event, event_type);
                             });
        };

    return base::ranges::count_if(received_events_,
                                  received_event_has_matching_event_type);
  }

  void StoreAccumulatedEvents() {
    std::string candidate_event;
    while (dom_message_queue_.PopMessage(&candidate_event)) {
      if (DOMDragEventWaiter::IsExpectedWindowName(candidate_event,
                                                   target_frame_name_)) {
        received_events_.push_back(candidate_event);
      }
    }
  }

  std::string target_frame_name_;
  content::DOMMessageQueue dom_message_queue_;
  std::vector<std::string> received_events_;
};

const char kTestPagePath[] = "/drag_and_drop/page.html";

// bool use_cross_site_subframe, double device_scale_factor.
using TestParam = std::tuple<bool, double>;

}  // namespace

// Note: Tests that require OS events need to be added to
// ozone-linux.interactive_ui_tests_wayland.filter.
class DragAndDropBrowserTest : public InProcessBrowserTest,
                               public testing::WithParamInterface<TestParam> {
 public:
  DragAndDropBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  DragAndDropBrowserTest(const DragAndDropBrowserTest&) = delete;
  DragAndDropBrowserTest& operator=(const DragAndDropBrowserTest&) = delete;

  struct DragImageBetweenFrames_TestState;
  void DragImageBetweenFrames_Start(bool image_same_origin,
                                    bool image_crossorigin_attr);
  void DragImageBetweenFrames_Step2(DragImageBetweenFrames_TestState*);
  void DragImageBetweenFrames_Step3(DragImageBetweenFrames_TestState*);

  struct DragImageFromDisappearingFrame_TestState;
  void DragImageFromDisappearingFrame_Step2(
      DragImageFromDisappearingFrame_TestState*);
  void DragImageFromDisappearingFrame_Step3(
      DragImageFromDisappearingFrame_TestState*);

  struct CrossSiteDrag_TestState;
  void CrossSiteDrag_Step2(CrossSiteDrag_TestState*);
  void CrossSiteDrag_Step3(CrossSiteDrag_TestState*);

  struct CrossNavCrossSiteDrag_TestState;
  void CrossNavCrossSiteDrag_Step2(CrossNavCrossSiteDrag_TestState*);
  void CrossNavCrossSiteDrag_Step3(CrossNavCrossSiteDrag_TestState*);

  struct CrossTabDrag_TestState;
  void CrossTabDrag_Step2(CrossTabDrag_TestState*);
  void CrossTabDrag_Step3(CrossTabDrag_TestState*);

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        "force-device-scale-factor",
        base::NumberToString(std::get<1>(GetParam())));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_test_server());
    ASSERT_TRUE(https_test_server()->Start());
    drag_simulator_ = std::make_unique<DragAndDropSimulator>(web_contents());
  }

  void TearDownOnMainThread() override {
    // For X11 need to tear down before UI goes away.
    drag_simulator_.reset();
  }

  bool use_cross_site_subframe() {
    // This is controlled by gtest's test param from INSTANTIATE_TEST_SUITE_P.
    return std::get<0>(GetParam());
  }

  content::RenderFrameHost* GetLeftFrame(
      content::WebContents* contents = nullptr) {
    AssertTestPageIsLoaded();
    return GetFrameByName("left", contents);
  }

  content::RenderFrameHost* GetRightFrame(
      content::WebContents* contents = nullptr) {
    AssertTestPageIsLoaded();
    return GetFrameByName("right", contents);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  //////////////////////
  // Navigation helpers.

  bool NavigateToTestPage(const std::string& origin) {
    return NavigateMainFrame(origin, kTestPagePath);
  }

  bool NavigateMainFrame(const std::string& origin, const std::string& path) {
    GURL url = https_test_server()->GetURL(origin, path);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return web_contents()->GetLastCommittedURL() == url;
  }

  // Navigates the left frame to |filename| (found under
  // chrome/test/data/drag_and_drop directory).
  bool NavigateLeftFrame(const std::string& frame_origin,
                         const std::string& image_origin,
                         bool image_crossorigin_attr,
                         const std::string& filename) {
    AssertTestPageIsLoaded();
    base::StringPairs replacement_text;
    replacement_text.push_back(
        std::make_pair("REPLACE_WITH_HOST_AND_PORT",
                       base::StringPrintf("%s:%d", image_origin.c_str(),
                                          https_test_server()->port())));
    replacement_text.push_back(std::make_pair(
        "REPLACE_WITH_CROSSORIGIN",
        std::string(image_crossorigin_attr ? "crossorigin" : "")));
    std::string path = net::test_server::GetFilePathWithReplacements(
        filename, replacement_text);
    return NavigateNamedFrame("left", frame_origin, path);
  }

  // Navigates the left frame to |filename| (found under
  // chrome/test/data/drag_and_drop directory).
  bool NavigateLeftFrame(const std::string& origin,
                         const std::string& filename) {
    return NavigateLeftFrame(origin, origin, false, filename);
  }

  // Navigates the right frame to |filename| (found under
  // chrome/test/data/drag_and_drop directory).
  bool NavigateRightFrame(const std::string& origin,
                          const std::string& filename) {
    AssertTestPageIsLoaded();
    return NavigateNamedFrame("right", origin, filename);
  }

  bool NavigateNamedFrame(const std::string& frame_name,
                          const std::string& origin,
                          const std::string& filename,
                          content::WebContents* web_contents = nullptr) {
    content::RenderFrameHost* frame = GetFrameByName(frame_name, web_contents);

    if (!frame) {
      return false;
    }

    // Navigate the frame and wait for the load event.
    std::string script = base::StringPrintf(
        "location.href = '/cross-site/%s/drag_and_drop/%s';\n"
        "new Promise(resolve => {"
        "  setTimeout(function() { resolve(42); }, 0);"
        "});",
        origin.c_str(), filename.c_str());
    content::TestFrameNavigationObserver observer(frame);
    if (content::EvalJs(frame, script).ExtractInt() != 42) {
      return false;
    }
    observer.Wait();

    // |frame| might have been swapped-out during a cross-site navigation,
    // therefore we need to get the current RenderFrameHost to work against
    // going forward.
    frame = web_contents ? GetFrameByName(frame_name, web_contents)
                         : GetFrameByName(frame_name);
    DCHECK(frame);

    // Wait until hit testing data is ready.
    WaitForHitTestData(frame);

    return true;
  }

  ////////////////////////////////////////////////////////////
  // Simulation of starting a drag-and-drop (using the mouse).

  bool SimulateMouseDownAndDragStartInLeftFrame() {
    AssertTestPageIsLoaded();

    // Waiting until the mousemove and mousedown events reach the right renderer
    // is needed to avoid flakiness reported in https://crbug.com/671445 (which
    // has its root cause in https://crbug.com/647378).  Once the latter bug
    // is fixed, we should no longer need to wait for these events (because
    // fixing https://crbug.com/647378 should guarantee that events arrive
    // to the renderer in the right order).
    DOMDragEventWaiter mouse_move_event_waiter("mousemove", GetLeftFrame());
    DOMDragEventWaiter mouse_down_event_waiter("mousedown", GetLeftFrame());

    if (!SimulateMouseMove(kMiddleOfLeftFrame)) {
      return false;
    }
    if (!mouse_move_event_waiter.WaitForNextMatchingEvent(nullptr)) {
      return false;
    }

    if (!SimulateMouseDown()) {
      return false;
    }
    if (!mouse_down_event_waiter.WaitForNextMatchingEvent(nullptr)) {
      return false;
    }

    if (!SimulateMouseMove(expected_location_of_drag_start_in_left_frame())) {
      return false;
    }

    return true;
  }

  gfx::Point expected_location_of_drag_start_in_left_frame() {
    // TODO(crbug.com/41279378): The delta below should exceed kDragThresholdX
    // and kDragThresholdY from MouseEventManager.cpp in blink.  Ideally, it
    // would come from the OS instead.
    return kMiddleOfLeftFrame + gfx::Vector2d(10, 10);
  }

  bool SimulateMouseMoveToLeftFrame() {
    AssertTestPageIsLoaded();
    return SimulateMouseMove(kMiddleOfLeftFrame);
  }

  bool SimulateMouseMoveToRightFrame() {
    AssertTestPageIsLoaded();
    return SimulateMouseMove(kMiddleOfRightFrame);
  }

  bool SimulateMouseUp() {
    return ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                              ui_controls::UP);
  }

  ////////////////////////////////////////////////////////////////////
  // Simulation of dragging from outside the browser into web contents
  // (using DragAndDropSimulator, not simulating mouse events).

  bool SimulateDragEnterToRightFrame(const std::string& text) {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDragEnter(kMiddleOfRightFrame, text);
  }

  bool SimulateDragEnterToRightFrame(const GURL& url) {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDragEnter(kMiddleOfRightFrame, url);
  }

  bool SimulateDragEnterToRightFrame(const base::FilePath& file) {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDragEnter(kMiddleOfRightFrame, file);
  }

  bool SimulateDropInRightFrame() {
    AssertTestPageIsLoaded();
    return drag_simulator_->SimulateDrop(kMiddleOfRightFrame);
  }

  bool SimulateDragEnterToOmnibox(const GURL& url) {
    AssertTestPageIsLoaded();
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    OmniboxViewViews* omnibox_view =
        browser_view->toolbar()->location_bar()->omnibox_view();

    gfx::Point point;
    views::View::ConvertPointToScreen(omnibox_view, &point);
    return drag_simulator_->SimulateOmniboxDragEnter(
        omnibox_view->GetWidget()->GetNativeView(), point, url);
  }

  bool SimulateDropInOmnibox() {
    AssertTestPageIsLoaded();
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    OmniboxViewViews* omnibox_view =
        browser_view->toolbar()->location_bar()->omnibox_view();

    gfx::Point point;
    views::View::ConvertPointToScreen(omnibox_view, &point);
    return drag_simulator_->SimulateOmniboxDrop(
        omnibox_view->GetWidget()->GetNativeView(), point);
  }

  gfx::Point GetMiddleOfRightFrameInScreenCoords() {
    aura::Window* window = web_contents()->GetNativeView();
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(window->GetRootWindow());
    gfx::Point screen_position(kMiddleOfRightFrame);
    if (screen_position_client) {
      screen_position_client->ConvertPointToScreen(window, &screen_position);
    }
    return screen_position;
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

 private:
  // Constants with coordinates within content/test/data/drag_and_drop/page.html
  // The precise frame center is at 200,200 and 400,200 coordinates, but slight
  // differences between left and right frame hopefully make it easier to detect
  // incorrect dom_drag_and_drop_event.clientX/Y values in test asserts.
  const gfx::Point kMiddleOfLeftFrame = gfx::Point(155, 150);
  const gfx::Point kMiddleOfRightFrame = gfx::Point(455, 250);

  bool SimulateMouseDown() {
    return ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                              ui_controls::DOWN);
  }

  bool SimulateMouseMove(const gfx::Point& location_inside_web_contents) {
    gfx::Rect bounds = web_contents()->GetContainerBounds();
    return ui_test_utils::SendMouseMoveSync(
        gfx::Point(bounds.x() + location_inside_web_contents.x(),
                   bounds.y() + location_inside_web_contents.y()));
  }

  content::RenderFrameHost* GetFrameByName(
      const std::string& name_to_find,
      content::WebContents* contents = nullptr) {
    return contents ? content::FrameMatchingPredicate(
                          contents->GetPrimaryPage(),
                          base::BindRepeating(&content::FrameMatchesName,
                                              name_to_find))
                    : content::FrameMatchingPredicate(
                          web_contents()->GetPrimaryPage(),
                          base::BindRepeating(&content::FrameMatchesName,
                                              name_to_find));
  }

  void AssertTestPageIsLoaded() {
    ASSERT_EQ(kTestPagePath, web_contents()->GetLastCommittedURL().path());
  }

  std::unique_ptr<DragAndDropSimulator> drag_simulator_;
  net::EmbeddedTestServer https_test_server_;
};

// Scenario: drag text from outside the browser and drop to the right frame.
// Test coverage: dragover, drop DOM events.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DropTextFromOutside) {
  // TODO (crbug/1521094): Test fails since 2023 refresh.
  if (std::get<double>(GetParam()) > 1.5) {
    GTEST_SKIP();
  }
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "drop_target.html"));

  // Setup test expectations.
  DOMDragEventVerifier expected_dom_event_data;
  expected_dom_event_data.set_expected_client_position("(155, 150)");
  expected_dom_event_data.set_expected_drop_effect("none");
  expected_dom_event_data.set_expected_effect_allowed("all");
  expected_dom_event_data.set_expected_mime_types("text/plain");
  expected_dom_event_data.set_expected_page_position("(155, 150)");

  // Drag text from outside the browser into/over the right frame.
  {
    DOMDragEventWaiter dragover_waiter("dragover", GetRightFrame());
    ASSERT_TRUE(SimulateDragEnterToRightFrame("Dragged test text"));

    std::string dragover_event;
    ASSERT_TRUE(dragover_waiter.WaitForNextMatchingEvent(&dragover_event));
    EXPECT_THAT(dragover_event, expected_dom_event_data.Matches());
  }

  // Allow the dragenter and dragover events to update the current drag
  // operations on the WebContentsView before proceeding to the drop, since
  // the latter needs that information to determine whether to force a
  // default action in the renderer process.
  base::RunLoop().RunUntilIdle();

  // Drop into the right frame.
  {
    DOMDragEventWaiter drop_waiter("drop", GetRightFrame());
    ASSERT_TRUE(SimulateDropInRightFrame());

    std::string drop_event;
    ASSERT_TRUE(drop_waiter.WaitForNextMatchingEvent(&drop_event));
    EXPECT_THAT(drop_event, expected_dom_event_data.Matches());
  }
}

// Scenario: drag URL from outside the browser and drop to the right frame
// (e.g. this is similar to a drag that starts from the bookmarks bar, except
// that here there is no drag start event - as-if the drag was started in
// another application).
//
// This test mostly focuses on covering 1) the navigation path, 2) focus
// behavior.  This test explicitly does not cover the dragover and/or drop DOM
// events - they are already covered via the DropTextFromOutside test above.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DropValidUrlFromOutside) {
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = web_contents->GetController();
  int initial_history_count = controller.GetEntryCount();
  GURL initial_url = web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Focus the omnibox.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Drag a normal URL from outside the browser into/over the right frame.
  GURL dragged_url = https_test_server()->GetURL("d.test", "/title2.html");
  ASSERT_TRUE(SimulateDragEnterToRightFrame(dragged_url));

  ui_test_utils::TabAddedWaiter wait_for_new_tab(browser());

  // Drop into the right frame.
  ASSERT_TRUE(SimulateDropInRightFrame());

  // Verify that dropping |dragged_url| creates a new tab and navigates it to
  // that URL.
  wait_for_new_tab.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver(new_web_contents, 1).Wait();
  EXPECT_EQ(dragged_url,
            new_web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the initial tab didn't navigate.
  EXPECT_EQ(initial_url,
            web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(initial_history_count, controller.GetEntryCount());

  // Verify that the focus moved from the omnibox to the tab contents.
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Scenario: drag a URL into the Omnibox.  This is a regression test for
// https://crbug.com/670123.
// TODO(crbug.com/344168586): Very flaky on linux-chromeos-rel bots.
#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(NDEBUG)
#define MAYBE_DropUrlIntoOmnibox DISABLED_DropUrlIntoOmnibox
#else
#define MAYBE_DropUrlIntoOmnibox DropUrlIntoOmnibox
#endif
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, MAYBE_DropUrlIntoOmnibox) {
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Focus the omnibox.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  OmniboxViewViews* omnibox_view =
      browser_view->toolbar()->location_bar()->omnibox_view();
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Click into Omnibox, so the text will be unselected.
  base::RunLoop loop1;
  ui_test_utils::MoveMouseToCenterAndPress(omnibox_view, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           loop1.QuitClosure());
  loop1.Run();
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Drag a normal URL from outside the browser into the Omnibox.
  GURL dragged_url = https_test_server()->GetURL("d.test", "/title2.html");
  ASSERT_TRUE(SimulateDragEnterToOmnibox(dragged_url));

  // Drop into the Omnibox.
  ASSERT_TRUE(SimulateDropInOmnibox());
  // Verify that no new tab is opened.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Verify that the dropped URL is selected.
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Click into the browser center to unselect the Omnibox text.
  base::RunLoop loop2;
  // The omnibox popup is open, and the browser's center point falls inside,
  // near the bottom of the popup. Offset the click down 5px, so that it
  // actually clicks the browser window and not the popup.
  ui_test_utils::MoveMouseToCenterWithOffsetAndPress(
      browser_view, {0, 5}, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, loop2.QuitClosure());
  loop2.Run();

  // Verify that the dropped URL is no longer selected after the mouse clicks
  // somewhere else.
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  EXPECT_FALSE(omnibox_view->drop_cursor_visible());
}

// Scenario: drag a file from outside the browser and drop to the right frame
// (e.g. starting a drag in a separate file explorer application, like Nemo on
// gLinux).
//
// This test mostly focuses on covering 1) the navigation path, 2) focus
// behavior.  This test explicitly does not cover the dragover and/or drop DOM
// events - they are already covered via the DropTextFromOutside test above.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DropFileFromOutside) {
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = web_contents->GetController();
  int initial_history_count = controller.GetEntryCount();
  GURL initial_url = web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Focus the omnibox.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Drag a file from outside the browser into/over the right frame.
  base::FilePath dragged_file = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath().AppendASCII("title3.html"));
  ASSERT_TRUE(SimulateDragEnterToRightFrame(dragged_file));

  ui_test_utils::TabAddedWaiter wait_for_new_tab(browser());

  // Drop into the right frame.
  ASSERT_TRUE(SimulateDropInRightFrame());

  // Verify that dropping |dragged_file| creates a new tab and navigates it to
  // the corresponding file: URL.
  wait_for_new_tab.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver(new_web_contents, 1).Wait();
  EXPECT_EQ(net::FilePathToFileURL(dragged_file),
            new_web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the initial tab didn't navigate.
  EXPECT_EQ(initial_url,
            web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(initial_history_count, controller.GetEntryCount());

  // Verify that the focus moved from the omnibox to the tab contents.
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Scenario: drag URL from outside the browser and drop to the right frame.
// Mostly focuses on covering the navigation path (the dragover and/or drop DOM
// events are already covered via the DropTextFromOutside test above).
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DropForbiddenUrlFromOutside) {
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "title1.html"));
  content::NavigationController& controller = web_contents()->GetController();
  int initial_history_count = controller.GetEntryCount();

  // Drag URL from outside the browser into/over the right frame.  The test uses
  // a URL that:
  // 1. Passes RenderWidgetHostImpl::FilterDropData checks.
  // 2. Fails CanDisplay checks in Blink (e.g. in RemoteFrame::Navigate).
  //    - This condition trigger the crash from https://crbug.com/1003169
  // 3. Passes BeginNavigation checks
  //    - This rules out "chrome-error://blah".
  GURL dragged_url("blob:null/some-guid");
  ASSERT_TRUE(SimulateDragEnterToRightFrame(dragged_url));

  // Drop into the right frame - this should *not* initiate navigating the main
  // frame to |dragged_url| (because this would be a forbidden, web->file
  // navigation).
  ASSERT_TRUE(SimulateDropInRightFrame());

  // Verify that the right frame is still responsive (this is a regression test
  // for https://crbug.com/1003169.
  ASSERT_TRUE(GetRightFrame()->GetProcess()->IsInitializedAndNotDead());
  EXPECT_EQ(123, content::EvalJs(GetRightFrame(), "123"));

  // Verify that the history remains unchanged.
  EXPECT_NE(dragged_url,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(initial_history_count, controller.GetEntryCount());
}

// Scenario: starting a drag in left frame
// Test coverage: dragstart DOM event, dragstart data passed to the OS.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DragStartInFrame) {
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateLeftFrame(frame_site, "image_source.html"));

  // Setup test expectations.
  DOMDragEventVerifier expected_dom_event_data;
  expected_dom_event_data.set_expected_client_position("(55, 50)");
  expected_dom_event_data.set_expected_drop_effect("none");
  // (dragstart event handler in image_source.html is asking for "copy" only).
  expected_dom_event_data.set_expected_effect_allowed("copy");
  expected_dom_event_data.set_expected_page_position("(55, 50)");

  // The dragstart event can have different mime types to the following events.
  // It is created by the renderer with the original DataTransfer object which
  // is then sent to the browser which initiates subsequent events.
  // The dragstart mime types will always include 'File', but access to the file
  // will not be allowed if the image is cross-origin (getAsFile() is null).
  // When the browser receives the DataTransfer, it copies data into
  // OSExchangeData and uses this for following events. Copying in text/html
  // will also populate text/plain. The File object may or may not be included
  // in following events depending on whether the image is cross-origin, and
  // whether the drop target is to a different page.
  expected_dom_event_data.set_expected_mime_types(
      "Files,text/html,text/uri-list");

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.SuppressPassingStartDragFurther();
  DOMDragEventWaiter dragstart_event_waiter("dragstart", GetLeftFrame());
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // Verify Javascript event data.
  {
    std::string dragstart_event;
    EXPECT_TRUE(
        dragstart_event_waiter.WaitForNextMatchingEvent(&dragstart_event));
    EXPECT_THAT(dragstart_event, expected_dom_event_data.Matches());
  }

  // Verify data being passed to the OS.
  {
    std::optional<url::Origin> source_origin;
    std::string text;
    std::string html;
    int operation = 0;
    gfx::Point location_inside_web_contents;
    drag_start_waiter.WaitUntilDragStart(&source_origin, &text, &html,
                                         &operation,
                                         &location_inside_web_contents);
    ASSERT_TRUE(source_origin.has_value());
    EXPECT_EQ(https_test_server()->GetOrigin(frame_site), source_origin);
    EXPECT_EQ(https_test_server()->GetURL(frame_site,
                                          "/drag_and_drop/cors-allowed.jpg"),
              text);
    EXPECT_THAT(
        html,
        testing::MatchesRegex(
            R"(<img .*src="https://.*/drag_and_drop/cors-allowed.jpg">)"));
    EXPECT_EQ(expected_location_of_drag_start_in_left_frame(),
              location_inside_web_contents);
    EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, operation);
  }

  // Try to leave everything in a clean state.
  SimulateMouseUp();
}

#if BUILDFLAG(IS_WIN)
// There is no known way to execute test-controlled tasks during
// a drag-and-drop loop run by Windows OS.
#define MAYBE_DragSameOriginImageBetweenFrames \
  DISABLED_DragSameOriginImageBetweenFrames
#elif BUILDFLAG(IS_LINUX)
// Failing to receive final drop event on linux crbug.com/1268407.
#define MAYBE_DragSameOriginImageBetweenFrames \
  DISABLED_DragSameOriginImageBetweenFrames
#else
#define MAYBE_DragSameOriginImageBetweenFrames DragSameOriginImageBetweenFrames
#endif

// Data that needs to be shared across multiple test steps below
// (i.e. across DragImageBetweenFrames_Step2 and DragImageBetweenFrames_Step3).
struct DragAndDropBrowserTest::DragImageBetweenFrames_TestState {
  bool expect_image_accessible = false;
  DOMDragEventVerifier expected_dom_event_data;
  std::unique_ptr<DOMDragEventWaiter> dragstart_event_waiter;
  std::unique_ptr<DOMDragEventWaiter> drop_event_waiter;
  std::unique_ptr<DOMDragEventWaiter> dragend_event_waiter;
  std::unique_ptr<DOMDragEventCounter> left_frame_events_counter;
  std::unique_ptr<DOMDragEventCounter> right_frame_events_counter;
};

// Scenario: drag a same-origin image from the left into the right frame.
// Test coverage: dragleave, dragenter, dragover, dragend, drop DOM events.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest,
                       MAYBE_DragSameOriginImageBetweenFrames) {
  // TODO (crbug/1521094): Test fails since 2023 refresh.
  if (std::get<1>(GetParam()) > 1.5) {
    GTEST_SKIP();
  }
  DragImageBetweenFrames_Start(/*image_same_origin=*/true,
                               /*image_crossorigin_attr=*/false);
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_DragCorsSameOriginImageBetweenFrames \
  DISABLED_DragCorsSameOriginImageBetweenFrames
#elif BUILDFLAG(IS_LINUX)
#define MAYBE_DragCorsSameOriginImageBetweenFrames \
  DISABLED_DragCorsSameOriginImageBetweenFrames
#else
#define MAYBE_DragCorsSameOriginImageBetweenFrames \
  DragCorsSameOriginImageBetweenFrames
#endif

// Scenario: drag a CORS same-orign (different origin with `<img crossorigin>`
// attribute, and `Access-Control-Allow-Origin: *` header) image from the left
// into the right frame. Image should be accessible.
// Test coverage: dragleave, dragenter, dragover, dragend, drop DOM events.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest,
                       MAYBE_DragCorsSameOriginImageBetweenFrames) {
  // TODO (crbug/1521094): Test fails since 2023 refresh.
  if (std::get<1>(GetParam()) > 1.5) {
    GTEST_SKIP();
  }
  DragImageBetweenFrames_Start(/*image_same_origin=*/false,
                               /*image_crossorigin_attr=*/true);
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_DragCrossOriginImageBetweenFrames \
  DISABLED_DragCrossOriginImageBetweenFrames
#elif BUILDFLAG(IS_LINUX)
#define MAYBE_DragCrossOriginImageBetweenFrames \
  DISABLED_DragCrossOriginImageBetweenFrames
#else
#define MAYBE_DragCrossOriginImageBetweenFrames \
  DragCrossOriginImageBetweenFrames
#endif

// Scenario: drag a cross-orign image from the left into the right frame. Image
// should not be accessible to the drag/drop events.
// Test coverage: dragleave, dragenter, dragover, dragend, drop DOM events.
// Regression test for https://crbug.com/1264873.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest,
                       MAYBE_DragCrossOriginImageBetweenFrames) {
  // TODO (crbug/1521094): Test fails since 2023 refresh.
  if (std::get<1>(GetParam()) > 1.5) {
    GTEST_SKIP();
  }
  DragImageBetweenFrames_Start(/*image_same_origin=*/false,
                               /*image_crossorigin_attr=*/false);
}

void DragAndDropBrowserTest::DragImageBetweenFrames_Start(
    bool image_same_origin,
    bool image_crossorigin_attr) {
  // Note that drag and drop will not expose data across cross-site frames on
  // the same page - this is why the same |frame_site| is used below both for
  // the left and the right frame.  See also https://crbug.com/59081.
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateLeftFrame(frame_site,
                                image_same_origin ? frame_site : "c.test",
                                image_crossorigin_attr, "image_source.html"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "drop_target.html"));

  // Setup test expectations.
  DragAndDropBrowserTest::DragImageBetweenFrames_TestState state;
  state.expect_image_accessible = image_same_origin || image_crossorigin_attr;
  state.left_frame_events_counter =
      std::make_unique<DOMDragEventCounter>(GetLeftFrame());
  state.right_frame_events_counter =
      std::make_unique<DOMDragEventCounter>(GetRightFrame());
  state.expected_dom_event_data.set_expected_client_position("(55, 50)");
  state.expected_dom_event_data.set_expected_drop_effect("none");
  // (dragstart event handler in image_source.html is asking for "copy" only).
  state.expected_dom_event_data.set_expected_effect_allowed("copy");
  state.expected_dom_event_data.set_expected_file_names(
      state.expect_image_accessible ? "cors-allowed.jpg" : "");
  state.expected_dom_event_data.set_expected_mime_types(
      "Files,text/html,text/uri-list");
  state.expected_dom_event_data.set_expected_page_position("(55, 50)");

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.PostTaskWhenDragStarts(
      base::BindOnce(&DragAndDropBrowserTest::DragImageBetweenFrames_Step2,
                     base::Unretained(this), base::Unretained(&state)));
  state.dragstart_event_waiter =
      std::make_unique<DOMDragEventWaiter>("dragstart", GetLeftFrame());
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // The next step of the test (DragImageBetweenFrames_Step2) runs inside the
  // nested drag-and-drop message loop - the call below won't return until the
  // drag-and-drop has already ended.
  drag_start_waiter.WaitUntilDragStart();

  DragImageBetweenFrames_Step3(&state);
}

void DragAndDropBrowserTest::DragImageBetweenFrames_Step2(
    DragAndDropBrowserTest::DragImageBetweenFrames_TestState* state) {
  // Verify dragstart DOM event.
  {
    std::string dragstart_event;
    EXPECT_TRUE(state->dragstart_event_waiter->WaitForNextMatchingEvent(
        &dragstart_event));
    EXPECT_THAT(dragstart_event, state->expected_dom_event_data.Matches());
    state->dragstart_event_waiter.reset();

    // Only a single "dragstart" should have fired in the left frame since the
    // start of the test.  We also allow any number of "dragover" events.
    EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                     "dragstart"));
    EXPECT_EQ(0, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                     {"dragleave", "dragenter", "drop", "dragend"}));

    // No events should have fired in the right frame yet.
    EXPECT_EQ(0, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                     {"dragstart", "dragleave", "dragenter", "dragover", "drop",
                      "dragend"}));
  }

  // While dragging, move mouse within the left frame.
  // Without this extra mouse move we wouldn't get a dragleave event later on.
  ASSERT_TRUE(SimulateMouseMoveToLeftFrame());

  // While dragging, move mouse from the left into the right frame.
  // This should trigger dragleave and dragenter events.
  {
    DOMDragEventWaiter dragleave_event_waiter("dragleave", GetLeftFrame());
    DOMDragEventWaiter dragenter_event_waiter("dragenter", GetRightFrame());
    state->left_frame_events_counter->Reset();
    state->right_frame_events_counter->Reset();
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragleave DOM event.
      std::string dragleave_event;

      state->expected_dom_event_data.set_expected_client_position("(355, 150)");
      state->expected_dom_event_data.set_expected_page_position("(355, 150)");
      state->expected_dom_event_data.set_expected_file_names("");
      state->expected_dom_event_data.set_expected_mime_types(
          state->expect_image_accessible
              ? "Files,text/html,text/plain,text/uri-list"
              : "text/html,text/plain,text/uri-list");

      EXPECT_TRUE(
          dragleave_event_waiter.WaitForNextMatchingEvent(&dragleave_event));
      EXPECT_THAT(dragleave_event, state->expected_dom_event_data.Matches());
    }

    {  // Verify dragenter DOM event.
      std::string dragenter_event;

      // Update expected event coordinates after SimulateMouseMoveToRightFrame
      // (these coordinates are relative to the right frame).
      state->expected_dom_event_data.set_expected_client_position("(155, 150)");
      state->expected_dom_event_data.set_expected_page_position("(155, 150)");

      EXPECT_TRUE(
          dragenter_event_waiter.WaitForNextMatchingEvent(&dragenter_event));
      EXPECT_THAT(dragenter_event, state->expected_dom_event_data.Matches());
    }

    // Note that ash (unlike aura/x11) will not fire dragover event in response
    // to the same mouse event that triggered a dragenter.  Because of that, we
    // postpone dragover testing until the next test step below.  See
    // implementation of ash::DragDropController::DragUpdate for details.
  }

  // Move the mouse twice in the right frame. The 1st move will ensure that
  // allowed operations communicated by the renderer will be stored in
  // WebContentsViewAura::current_drag_data_. The 2nd move will ensure that this
  // gets copied into DesktopDragDropClientAuraX11::negotiated_operation_.
  for (int i = 0; i < 2; i++) {
    DOMDragEventWaiter dragover_event_waiter("dragover", GetRightFrame());
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragover DOM event.
      std::string dragover_event;
      EXPECT_TRUE(
          dragover_event_waiter.WaitForNextMatchingEvent(&dragover_event));
      EXPECT_THAT(dragover_event, state->expected_dom_event_data.Matches());
    }
  }

  // Only a single "dragleave" should have fired in the left frame since the
  // last checkpoint.  We also allow any number of "dragover" events.
  EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragleave"));
  EXPECT_EQ(0, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragenter", "drop", "dragend"}));

  // A single "dragenter" + at least one "dragover" event should have fired in
  // the right frame since the last checkpoint.
  EXPECT_EQ(1, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragenter"));
  EXPECT_LE(1, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragover"));
  EXPECT_EQ(0, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragleave", "drop", "dragend"}));

  // Release the mouse button to end the drag.
  state->drop_event_waiter =
      std::make_unique<DOMDragEventWaiter>("drop", GetRightFrame());
  state->dragend_event_waiter =
      std::make_unique<DOMDragEventWaiter>("dragend", GetLeftFrame());
  state->left_frame_events_counter->Reset();
  state->right_frame_events_counter->Reset();
  SimulateMouseUp();
  // The test will continue in DragImageBetweenFrames_Step3.
}

void DragAndDropBrowserTest::DragImageBetweenFrames_Step3(
    DragAndDropBrowserTest::DragImageBetweenFrames_TestState* state) {
  // Verify drop DOM event.
  {
    // File contents is sent in drop event.
    state->expected_dom_event_data.set_expected_file_names(
        state->expect_image_accessible ? "cors-allowed.jpg" : "");
    state->expected_dom_event_data.set_expected_mime_types(
        state->expect_image_accessible
            ? "Files,text/html,text/plain,text/uri-list"
            : "text/html,text/plain,text/uri-list");

    std::string drop_event;
    EXPECT_TRUE(
        state->drop_event_waiter->WaitForNextMatchingEvent(&drop_event));
    state->drop_event_waiter.reset();
    EXPECT_THAT(drop_event, state->expected_dom_event_data.Matches());
  }

  // Verify dragend DOM event.
  {
    // Different values of DataTransfer.dropEffect is observed and is
    // being tracked by https://crbug.com/1470718.
    // Different values of DataTransfer.types is seen due to
    // https://crbug.com/394955. This causes certain File objects to be
    // mapped to text/plain in `DataObject::ToWebDragData()` and thus
    // text/plain is seen in "dragleave", "dragenter", "dragover" and "drop"
    // events. While dragend doesn't use WebDragData object and that is why
    // text/plain is not seen in this event.
    state->expected_dom_event_data.set_expected_drop_effect("copy");
    state->expected_dom_event_data.set_expected_mime_types(
        "Files,text/html,text/uri-list");

    // TODO: https://crbug.com/686136: dragEnd coordinates for non-OOPIF
    // scenarios are currently broken.
    state->expected_dom_event_data.set_expected_client_position(
        "<no expectation>");
    state->expected_dom_event_data.set_expected_page_position(
        "<no expectation>");
    // File contents is not sent in dragend.
    state->expected_dom_event_data.set_expected_file_names("");

    std::string dragend_event;
    EXPECT_TRUE(
        state->dragend_event_waiter->WaitForNextMatchingEvent(&dragend_event));
    state->dragend_event_waiter.reset();
    EXPECT_THAT(dragend_event, state->expected_dom_event_data.Matches());
  }

  // Only a single "dragend" should have fired in the left frame since the last
  // checkpoint.
  EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragend"));
  EXPECT_EQ(0,
            state->left_frame_events_counter->GetNumberOfReceivedEvents(
                {"dragstart", "dragleave", "dragenter", "dragover", "drop"}));

  // A single "drop" + possibly some "dragover" events should have fired in the
  // right frame since the last checkpoint.
  EXPECT_EQ(
      1, state->right_frame_events_counter->GetNumberOfReceivedEvents("drop"));
  EXPECT_EQ(0, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragleave", "dragenter", "dragend"}));
}

// There is no known way to execute test-controlled tasks during
// a drag-and-drop loop run by Windows OS.
// Also disable the test on Linux due to flaky: crbug.com/1164442
// TODO(crbug.com/40118868): Revisit once build flag switch of lacros-chrome is
// complete.
// TODO(crbug.com/40876472): Enable on ChromeOS ASAN once flakiness is fixed.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) ||            \
    (BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER))
#define MAYBE_DragImageFromDisappearingFrame \
  DISABLED_DragImageFromDisappearingFrame
#else
#define MAYBE_DragImageFromDisappearingFrame DragImageFromDisappearingFrame
#endif

// Data that needs to be shared across multiple test steps below
// (i.e. across DragImageFromDisappearingFrame_Step2 and
// DragImageFromDisappearingFrame_Step3).
struct DragAndDropBrowserTest::DragImageFromDisappearingFrame_TestState {
  DOMDragEventVerifier expected_dom_event_data;
  std::unique_ptr<DOMDragEventWaiter> drop_event_waiter;
};

// Scenario: drag an image from the left into the right frame and delete the
// left frame during the drag.  This is a regression test for
// https://crbug.com/670123.
// Test coverage: dragenter, dragover, drop DOM events.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest,
                       MAYBE_DragImageFromDisappearingFrame) {
  // TODO (crbug/1521094): Test fails since 2023 refresh.
  if (std::get<1>(GetParam()) > 1.5) {
    GTEST_SKIP();
  }
  // Load the test page.
  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateLeftFrame(frame_site, "image_source.html"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "drop_target.html"));

  // Setup test expectations.
  DragAndDropBrowserTest::DragImageFromDisappearingFrame_TestState state;
  state.expected_dom_event_data.set_expected_drop_effect("none");
  // (dragstart event handler in image_source.html is asking for "copy" only).
  state.expected_dom_event_data.set_expected_effect_allowed("copy");
  state.expected_dom_event_data.set_expected_mime_types(
      "Files,text/html,text/plain,text/uri-list");
  state.expected_dom_event_data.set_expected_client_position("(155, 150)");
  state.expected_dom_event_data.set_expected_page_position("(155, 150)");

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.PostTaskWhenDragStarts(base::BindOnce(
      &DragAndDropBrowserTest::DragImageFromDisappearingFrame_Step2,
      base::Unretained(this), base::Unretained(&state)));
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // The next step of the test (DragImageFromDisappearingFrame_Step2) runs
  // inside the nested drag-and-drop message loop - the call below won't return
  // until the drag-and-drop has already ended.
  drag_start_waiter.WaitUntilDragStart();

  DragImageFromDisappearingFrame_Step3(&state);
}

void DragAndDropBrowserTest::DragImageFromDisappearingFrame_Step2(
    DragAndDropBrowserTest::DragImageFromDisappearingFrame_TestState* state) {
  // Delete the left frame in an attempt to repro https://crbug.com/670123.
  content::RenderFrameDeletedObserver frame_deleted_observer(GetLeftFrame());
  ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "frame = document.getElementById('left');\n"
                     "frame.parentNode.removeChild(frame);\n"));
  frame_deleted_observer.WaitUntilDeleted();

  // While dragging, move mouse from the left into the right frame.
  // This should trigger dragleave and dragenter events.
  {
    DOMDragEventWaiter dragenter_event_waiter("dragenter", GetRightFrame());
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragenter DOM event.
      std::string dragenter_event;
      EXPECT_TRUE(
          dragenter_event_waiter.WaitForNextMatchingEvent(&dragenter_event));
      EXPECT_THAT(dragenter_event, state->expected_dom_event_data.Matches());
    }

    // Note that ash (unlike aura/x11) will not fire dragover event in response
    // to the same mouse event that triggered a dragenter.  Because of that, we
    // postpone dragover testing until the next test step below.  See
    // implementation of ash::DragDropController::DragUpdate for details.
  }

  // Move the mouse twice in the right frame. The 1st move will ensure that
  // allowed operations communicated by the renderer will be stored in
  // WebContentsViewAura::current_drag_data_. The 2nd move will ensure that this
  // gets copied into DesktopDragDropClientAuraX11::negotiated_operation_.
  for (int i = 0; i < 2; i++) {
    DOMDragEventWaiter dragover_event_waiter("dragover", GetRightFrame());
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragover DOM event.
      std::string dragover_event;
      EXPECT_TRUE(
          dragover_event_waiter.WaitForNextMatchingEvent(&dragover_event));
      EXPECT_THAT(dragover_event, state->expected_dom_event_data.Matches());
    }
  }

  // Release the mouse button to end the drag.
  state->drop_event_waiter =
      std::make_unique<DOMDragEventWaiter>("drop", GetRightFrame());
  SimulateMouseUp();
  // The test will continue in DragImageFromDisappearingFrame_Step3.
}

void DragAndDropBrowserTest::DragImageFromDisappearingFrame_Step3(
    DragAndDropBrowserTest::DragImageFromDisappearingFrame_TestState* state) {
  // Verify drop DOM event.
  {
    std::string drop_event;
    EXPECT_TRUE(
        state->drop_event_waiter->WaitForNextMatchingEvent(&drop_event));
    state->drop_event_waiter.reset();
    EXPECT_THAT(drop_event, state->expected_dom_event_data.Matches());
  }
}

// There is no known way to execute test-controlled tasks during
// a drag-and-drop loop run by Windows OS.
// TODO(b:361552512): Flaky on Chrome OS
#if BUILDFLAG(IS_WIN)
#define MAYBE_CrossSiteDrag DISABLED_CrossSiteDrag
#elif BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CrossSiteDrag DISABLED_CrossSiteDrag
#else
#define MAYBE_CrossSiteDrag CrossSiteDrag
#endif

// Data that needs to be shared across multiple test steps below
// (i.e. across CrossSiteDrag_Step2 and CrossSiteDrag_Step3).
struct DragAndDropBrowserTest::CrossSiteDrag_TestState {
  std::unique_ptr<DOMDragEventWaiter> dragend_event_waiter;
  std::unique_ptr<DOMDragEventCounter> left_frame_events_counter;
  std::unique_ptr<DOMDragEventCounter> right_frame_events_counter;
};

// Scenario: drag an image from the left into the right frame when the
// left-vs-right frames are cross-site.  This is a regression test for
// https://crbug.com/59081.
//
// Test coverage: absence of dragenter, dragover, drop DOM events
// + presence of dragstart, dragleave and dragend.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, MAYBE_CrossSiteDrag) {
  std::string left_frame_site = "c.test";  // Always cross-site VS main frame.
  std::string right_frame_site =
      use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateLeftFrame(left_frame_site, "image_source.html"));
  ASSERT_TRUE(NavigateRightFrame(right_frame_site, "drop_target.html"));

  // Setup test expectations.
  DragAndDropBrowserTest::CrossSiteDrag_TestState state;
  state.left_frame_events_counter =
      std::make_unique<DOMDragEventCounter>(GetLeftFrame());
  state.right_frame_events_counter =
      std::make_unique<DOMDragEventCounter>(GetRightFrame());

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.PostTaskWhenDragStarts(
      base::BindOnce(&DragAndDropBrowserTest::CrossSiteDrag_Step2,
                     base::Unretained(this), base::Unretained(&state)));
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // The next step of the test (CrossSiteDrag_Step2) runs inside the
  // nested drag-and-drop message loop - the call below won't return until the
  // drag-and-drop has already ended.
  drag_start_waiter.WaitUntilDragStart();

  CrossSiteDrag_Step3(&state);
}

void DragAndDropBrowserTest::CrossSiteDrag_Step2(
    DragAndDropBrowserTest::CrossSiteDrag_TestState* state) {
  // While "dragleave" and "drop" events are not expected in this test, we
  // simulate extra mouse operations for consistency with
  // DragImageBetweenFrames_Step2.
  ASSERT_TRUE(SimulateMouseMoveToLeftFrame());
  for (int i = 0; i < 3; i++) {
    content::DOMMessageQueue dom_message_queue(web_contents());
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    // No events are expected from the right frame, so we can't wait for a
    // dragover event here.  Still - we do want to wait until the right frame
    // has had a chance to process any previous browser IPCs, so that in case
    // there *is* a bug and a dragover event *does* happen, we won't terminate
    // the test before the event has had a chance to be reported back to the
    // browser.
    std::string expected_response = base::StringPrintf("\"i%d\"", i);
    GetRightFrame()->ExecuteJavaScriptWithUserGestureForTests(
        base::UTF8ToUTF16(base::StringPrintf(
            "domAutomationController.send(%s);", expected_response.c_str())),
        base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);

    // Wait until our response comes back (it might be mixed with responses
    // carrying events that are sent by event_monitoring.js).
    std::string actual_response;
    do {
      ASSERT_TRUE(dom_message_queue.WaitForMessage(&actual_response));
    } while (actual_response != expected_response);
  }

  // Release the mouse button to end the drag.
  state->dragend_event_waiter =
      std::make_unique<DOMDragEventWaiter>("dragend", GetLeftFrame());
  SimulateMouseUp();
  // The test will continue in CrossSiteDrag_Step3.
}

void DragAndDropBrowserTest::CrossSiteDrag_Step3(
    DragAndDropBrowserTest::CrossSiteDrag_TestState* state) {
  EXPECT_TRUE(state->dragend_event_waiter->WaitForNextMatchingEvent(nullptr));

  // Since the start of the test the left frame should have seen a single
  // "dragstart", and a "dragend" event (and possibly a "dragleave" and some
  // "dragover" events).
  EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragstart"));
  EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragend"));
  EXPECT_EQ(
      0, state->left_frame_events_counter->GetNumberOfReceivedEvents("drop"));

  // No events should have fired in the right frame, because it is cross-site
  // from the source of the drag.  This is the essence of this test.
  EXPECT_EQ(0, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragleave", "dragenter", "dragover", "drop",
                    "dragend"}));
}

// There is no known way to execute test-controlled tasks during
// a drag-and-drop loop run by Windows OS.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CrossNavCrossSiteDrag DISABLED_CrossNavCrossSiteDrag
#else
#define MAYBE_CrossNavCrossSiteDrag CrossNavCrossSiteDrag
#endif

struct DragAndDropBrowserTest::CrossNavCrossSiteDrag_TestState {
  // Tracks events in the left frame of the initial site.
  std::unique_ptr<DOMDragEventCounter> left_frame_events_counter;
  // Tracks events in the main frame of the navigated-to site.
  std::unique_ptr<DOMDragEventCounter> post_nav_events_counter;
};

// Scenario: drag from a cross-site frame, navigate the main frame, then drop.
// This is a regression test for https://crbug.com/1485266.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, MAYBE_CrossNavCrossSiteDrag) {
  std::string left_frame_site = "b.test";  // Always cross-site VS main frame.
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateLeftFrame(left_frame_site, "image_source.html"));

  // Setup test expectations.
  DragAndDropBrowserTest::CrossNavCrossSiteDrag_TestState state;
  state.left_frame_events_counter =
      std::make_unique<DOMDragEventCounter>(GetLeftFrame());

  // Start the drag in the left frame.
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.PostTaskWhenDragStarts(
      base::BindOnce(&DragAndDropBrowserTest::CrossNavCrossSiteDrag_Step2,
                     base::Unretained(this), base::Unretained(&state)));
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // The next step of the test (CrossNavCrossSiteDrag_Step2) runs inside the
  // nested drag-and-drop message loop - the call below won't return until the
  // drag-and-drop has already ended.
  drag_start_waiter.WaitUntilDragStart();

  CrossNavCrossSiteDrag_Step3(&state);
}

void DragAndDropBrowserTest::CrossNavCrossSiteDrag_Step2(
    DragAndDropBrowserTest::CrossNavCrossSiteDrag_TestState* state) {
  // Drag has started; navigate the main frame and make sure the RVH has changed
  // (to validate this test is working as intended).
  content::RenderViewHost* rvh_before_nav =
      web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
  ASSERT_TRUE(NavigateMainFrame("c.test", "/drag_and_drop/drop_target.html"));
  EXPECT_NE(rvh_before_nav,
            web_contents()->GetPrimaryMainFrame()->GetRenderViewHost());
  state->post_nav_events_counter = std::make_unique<DOMDragEventCounter>(
      web_contents()->GetPrimaryMainFrame());

  // While "dragleave" and "drop" events are not expected in this test, we
  // simulate extra mouse operations for consistency with
  // DragImageBetweenFrames_Step2.
  ASSERT_TRUE(SimulateMouseMove(gfx::Point(15, 15)));
  for (int i = 0; i < 3; i++) {
    content::DOMMessageQueue dom_message_queue(web_contents());
    ASSERT_TRUE(SimulateMouseMove(gfx::Point(15, 16 + i)));

    // No events are expected from the right frame, so we can't wait for a
    // dragover event here.  Still - we do want to wait until the right frame
    // has had a chance to process any previous browser IPCs, so that in case
    // there *is* a bug and a dragover event *does* happen, we won't terminate
    // the test before the event has had a chance to be reported back to the
    // browser.
    std::string expected_response = base::StringPrintf("\"i%d\"", i);
    web_contents()
        ->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            base::UTF8ToUTF16(
                base::StringPrintf("domAutomationController.send(%s);",
                                   expected_response.c_str())),
            base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);

    // Wait until our response comes back (it might be mixed with responses
    // carrying events that are sent by event_monitoring.js).
    std::string actual_response;
    do {
      ASSERT_TRUE(dom_message_queue.WaitForMessage(&actual_response));
    } while (actual_response != expected_response);
  }

  // Release the mouse button to end the drag.
  SimulateMouseUp();
  // The test will continue in CrossNavCrossSiteDrag_Step3.
}

void DragAndDropBrowserTest::CrossNavCrossSiteDrag_Step3(
    DragAndDropBrowserTest::CrossNavCrossSiteDrag_TestState* state) {
  // Since the start of the test the left frame should have seen a single
  // "dragstart". The "dragend" may be missing since the navigation happened in
  // the middle of the drag.
  EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragstart"));
  EXPECT_EQ(
      0, state->left_frame_events_counter->GetNumberOfReceivedEvents("drop"));

  // No events should have fired in the site, because it is cross-site
  // from the source of the drag but is in the same tab. This is the essence of
  // this test.
  EXPECT_EQ(0, state->post_nav_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragleave", "dragenter", "dragover", "drop",
                    "dragend"}));
}

// There is no known way to execute test-controlled tasks during
// a drag-and-drop loop run by Windows OS.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_CHROMEOS_ASH) && \
                          (defined(ADDRESS_SANITIZER) || !defined(NDEBUG)))
// https://crbug.com/1393605: Flaky at ChromeOS ASAN and Debug builds
#define MAYBE_CrossTabDrag DISABLED_CrossTabDrag
#else
#define MAYBE_CrossTabDrag CrossTabDrag
#endif

// Data that needs to be shared across multiple test steps below
// (i.e. across CrossTabDrag_Step2 and CrossTabDrag_Step3).
struct DragAndDropBrowserTest::CrossTabDrag_TestState {
  DOMDragEventVerifier expected_dom_event_data;
  std::unique_ptr<DOMDragEventWaiter> dragstart_event_waiter;
  std::unique_ptr<DOMDragEventWaiter> drop_event_waiter;
  std::unique_ptr<DOMDragEventWaiter> dragend_event_waiter;
  std::unique_ptr<DOMDragEventCounter> left_frame_events_counter;
  std::unique_ptr<DOMDragEventCounter> right_frame_events_counter;
};

// Scenario: drag an image from one tab to a different cross-site tab. This
// simulates starting a drag, switching tabs as in alt-tab, then completing the
// drop.
// right frames are on different tabs.
// Note that this case is allowed, while the CrossSiteDrag case (cross-site
// same-tab drag) is not allowed.
//
// Test coverage: dragenter, dragover, dragend, drop DOM events.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, MAYBE_CrossTabDrag) {
  // TODO (crbug/1521094): Test fails since 2023 refresh.
  if (std::get<1>(GetParam()) > 1.5) {
    GTEST_SKIP();
  }
  std::string right_frame_site =
      use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateLeftFrame("c.test", "image_source.html"));

  // Add a new tab navigated to the test page, and navigate the right subframe.
  GURL url = https_test_server()->GetURL("a.test", kTestPagePath);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  content::WebContents* first_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* second_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(NavigateNamedFrame("right", right_frame_site, "drop_target.html",
                                 second_contents));

  // Setup test expectations.
  DragAndDropBrowserTest::CrossTabDrag_TestState state;
  state.left_frame_events_counter =
      std::make_unique<DOMDragEventCounter>(GetLeftFrame(first_contents));
  state.right_frame_events_counter =
      std::make_unique<DOMDragEventCounter>(GetRightFrame(second_contents));
  state.expected_dom_event_data.set_expected_client_position("(55, 50)");
  state.expected_dom_event_data.set_expected_drop_effect("none");
  // (dragstart event handler in image_source.html is asking for "copy" only).
  state.expected_dom_event_data.set_expected_effect_allowed("copy");
  state.expected_dom_event_data.set_expected_mime_types(
      "Files,text/html,text/plain,text/uri-list");
  state.expected_dom_event_data.set_expected_page_position("(55, 50)");

  // Make the first tab active, then start the drag in the left frame.
  browser()->tab_strip_model()->ActivateTabAt(0);
  DragStartWaiter drag_start_waiter(web_contents());
  drag_start_waiter.PostTaskWhenDragStarts(
      base::BindOnce(&DragAndDropBrowserTest::CrossTabDrag_Step2,
                     base::Unretained(this), base::Unretained(&state)));
  state.dragstart_event_waiter = std::make_unique<DOMDragEventWaiter>(
      "dragstart", GetLeftFrame(first_contents));
  EXPECT_TRUE(SimulateMouseDownAndDragStartInLeftFrame());

  // The next step of the test (CrossTabDrag_Step2) runs inside the
  // nested drag-and-drop message loop - the call below won't return until the
  // drag-and-drop has already ended.
  drag_start_waiter.WaitUntilDragStart();

  CrossTabDrag_Step3(&state);
}

void DragAndDropBrowserTest::CrossTabDrag_Step2(
    DragAndDropBrowserTest::CrossTabDrag_TestState* state) {
  browser()->tab_strip_model()->ActivateTabAt(0);
  content::WebContents* first_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* second_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  // Verify dragstart DOM event.
  {
    std::string dragstart_event;
    EXPECT_TRUE(state->dragstart_event_waiter->WaitForNextMatchingEvent(
        &dragstart_event));
    state->dragstart_event_waiter.reset();

    // Only a single "dragstart" should have fired in the left frame since the
    // start of the test.  We also allow any number of "dragover" events.
    EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                     "dragstart"));
    EXPECT_EQ(0, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                     {"dragleave", "dragenter", "drop", "dragend"}));

    // No events should have fired in the right frame yet.
    EXPECT_EQ(0, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                     {"dragstart", "dragleave", "dragenter", "dragover", "drop",
                      "dragend"}));
  }

  // While dragging, move mouse from the left into the right frame.
  // This should trigger a dragenter event.
  {
    DOMDragEventWaiter dragenter_event_waiter("dragenter",
                                              GetRightFrame(second_contents));
    state->left_frame_events_counter->Reset();
    state->right_frame_events_counter->Reset();

    // Switch tabs before moving mouse to right frame. This simulates the case
    // where a drag is started, the user pressed alt+tab, then completes the
    // drop.
    browser()->tab_strip_model()->ActivateTabAt(1);
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragenter DOM event.
      std::string dragenter_event;

      // Update expected event coordinates after SimulateMouseMoveToRightFrame
      // (these coordinates are relative to the right frame).
      state->expected_dom_event_data.set_expected_client_position("(155, 150)");
      state->expected_dom_event_data.set_expected_page_position("(155, 150)");

      EXPECT_TRUE(
          dragenter_event_waiter.WaitForNextMatchingEvent(&dragenter_event));
      EXPECT_THAT(dragenter_event, state->expected_dom_event_data.Matches());
    }

    // Note that ash (unlike aura/x11) will not fire dragover event in response
    // to the same mouse event that triggered a dragenter.  Because of that, we
    // postpone dragover testing until the next test step below.  See
    // implementation of ash::DragDropController::DragUpdate for details.
  }

  // Move the mouse twice in the right frame. The 1st move will ensure that
  // allowed operations communicated by the renderer will be stored in
  // WebContentsViewAura::current_drag_data_. The 2nd move will ensure that this
  // gets copied into DesktopDragDropClientAuraX11::negotiated_operation_.
  for (int i = 0; i < 2; i++) {
    DOMDragEventWaiter dragover_event_waiter("dragover", GetRightFrame());
    ASSERT_TRUE(SimulateMouseMoveToRightFrame());

    {  // Verify dragover DOM event.
      std::string dragover_event;
      EXPECT_TRUE(
          dragover_event_waiter.WaitForNextMatchingEvent(&dragover_event));
      EXPECT_THAT(dragover_event, state->expected_dom_event_data.Matches());
    }
  }

  // We allow any number of "dragover" events, but should not have any of the
  // listed events.
  EXPECT_EQ(0, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragenter", "drop", "dragend"}));

  // A single "dragenter" + at least one "dragover" event should have fired in
  // the right frame since the last checkpoint.
  EXPECT_EQ(1, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragenter"));
  EXPECT_LE(1, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragover"));
  EXPECT_EQ(0, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragleave", "drop", "dragend"}));

  // Release the mouse button to end the drag.
  state->drop_event_waiter = std::make_unique<DOMDragEventWaiter>(
      "drop", GetRightFrame(second_contents));
  state->dragend_event_waiter = std::make_unique<DOMDragEventWaiter>(
      "dragend", GetLeftFrame(first_contents));
  state->left_frame_events_counter->Reset();
  state->right_frame_events_counter->Reset();
  SimulateMouseUp();
  // The test will continue in CrossTabDrag_Step3.
}

void DragAndDropBrowserTest::CrossTabDrag_Step3(
    DragAndDropBrowserTest::CrossTabDrag_TestState* state) {
  // Verify drop DOM event.
  {
    std::string drop_event;
    EXPECT_TRUE(
        state->drop_event_waiter->WaitForNextMatchingEvent(&drop_event));
    state->drop_event_waiter.reset();
    EXPECT_THAT(drop_event, state->expected_dom_event_data.Matches());
  }

  // Verify dragend DOM event.
  {
    // Different values of DataTransfer.dropEffect is observed and is
    // being tracked by https://crbug.com/1470718.
    // Different values of DataTransfer.types is seen due to
    // https://crbug.com/394955. This causes certain File objects to be
    // mapped to text/plain in `DataObject::ToWebDragData()` and thus
    // text/plain is seen in "dragleave", "dragenter", "dragover" and "drop"
    // events. While dragend doesn't use WebDragData object and that is why
    // text/plain is not seen in this event.
    state->expected_dom_event_data.set_expected_drop_effect("copy");
    state->expected_dom_event_data.set_expected_mime_types(
        "Files,text/html,text/uri-list");

    // TODO: https://crbug.com/686136: dragEnd coordinates for non-OOPIF
    // scenarios are currently broken.
    state->expected_dom_event_data.set_expected_client_position(
        "<no expectation>");
    state->expected_dom_event_data.set_expected_page_position(
        "<no expectation>");

    std::string dragend_event;
    EXPECT_TRUE(
        state->dragend_event_waiter->WaitForNextMatchingEvent(&dragend_event));
    state->dragend_event_waiter.reset();
    EXPECT_THAT(dragend_event, state->expected_dom_event_data.Matches());
  }

  // Only a single "dragend" should have fired in the left frame since the last
  // checkpoint.
  EXPECT_EQ(1, state->left_frame_events_counter->GetNumberOfReceivedEvents(
                   "dragend"));
  EXPECT_EQ(0,
            state->left_frame_events_counter->GetNumberOfReceivedEvents(
                {"dragstart", "dragleave", "dragenter", "dragover", "drop"}));

  // A single "drop" + possibly some "dragover" events should have fired in the
  // right frame since the last checkpoint.
  EXPECT_EQ(
      1, state->right_frame_events_counter->GetNumberOfReceivedEvents("drop"));
  EXPECT_EQ(0, state->right_frame_events_counter->GetNumberOfReceivedEvents(
                   {"dragstart", "dragleave", "dragenter", "dragend"}));
}

// Test that screenX/screenY for drag updates are in screen coordinates.
// See https://crbug.com/600402 where we mistook the root window coordinate
// space for the screen coordinate space.
IN_PROC_BROWSER_TEST_P(DragAndDropBrowserTest, DragUpdateScreenCoordinates) {
  // Reposition the window so that the root window coordinate space and the
  // screen coordinate space are clearly distinct. Otherwise this test would
  // be inconclusive.
  // In addition to offsetting the window, use a small window size to avoid
  // rejection of the new bounds by the system.
  browser()->window()->SetBounds(gfx::Rect(200, 100, 700, 500));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser()->window()->GetBounds().origin() == gfx::Point(200, 100);
  }));

  std::string frame_site = use_cross_site_subframe() ? "b.test" : "a.test";
  ASSERT_TRUE(NavigateToTestPage("a.test"));
  ASSERT_TRUE(NavigateRightFrame(frame_site, "drop_target.html"));

  const gfx::Point screen_position = GetMiddleOfRightFrameInScreenCoords();

  DOMDragEventVerifier expected_dom_event_data;
  expected_dom_event_data.set_expected_client_position("(155, 150)");
  expected_dom_event_data.set_expected_screen_position(
      base::StringPrintf("(%d, %d)", screen_position.x(), screen_position.y()));

  DOMDragEventWaiter dragover_waiter("dragover", GetRightFrame());
  ASSERT_TRUE(SimulateDragEnterToRightFrame("Dragged test text"));

  std::string dragover_event;
  ASSERT_TRUE(dragover_waiter.WaitForNextMatchingEvent(&dragover_event));
  EXPECT_THAT(dragover_event, expected_dom_event_data.Matches());
}

// TODO(paulmeyer): Should test the case of navigation happening in the middle
// of a drag operation, and cross-site drags should be allowed across a
// navigation.

// Injecting input with scaling works as expected on Chromeos.
// TODO(crbug.com/40231833): Enable tests with a scale factor on lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr std::initializer_list<double> ui_scaling_factors = {1.0, 1.25, 2.0};
#else
// Injecting input with non-1x scaling doesn't work correctly with x11 ozone or
// Windows 7.
constexpr std::initializer_list<double> ui_scaling_factors = {1.0};
#endif

INSTANTIATE_TEST_SUITE_P(
    SameSiteSubframe,
    DragAndDropBrowserTest,
    ::testing::Combine(::testing::Values(false),
                       ::testing::ValuesIn(ui_scaling_factors)));

INSTANTIATE_TEST_SUITE_P(
    CrossSiteSubframe,
    DragAndDropBrowserTest,
    ::testing::Combine(::testing::Values(true),
                       ::testing::ValuesIn(ui_scaling_factors)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DragAndDropBrowserTestNoParam : public InProcessBrowserTest {
 protected:
  void SimulateDragFromOmniboxToWebContents(base::OnceClosure quit) {
    chrome::FocusLocationBar(browser());

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    OmniboxViewViews* omnibox_view =
        browser_view->toolbar()->location_bar()->omnibox_view();

    // Simulate mouse move to omnibox.
    gfx::Point point;
    views::View::ConvertPointToScreen(omnibox_view, &point);
    EXPECT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        point.x(), point.y(),
        base::BindOnce(&DragAndDropBrowserTestNoParam::Step2,
                       base::Unretained(this), std::move(quit))));
  }

  void Step2(base::OnceClosure quit) {
    // Simulate mouse down.
    EXPECT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::DOWN,
        base::BindOnce(&DragAndDropBrowserTestNoParam::Step3,
                       base::Unretained(this), std::move(quit))));
  }

  void Step3(base::OnceClosure quit) {
    // Simulate mouse move to WebContents.
    // Keep sending mouse move until the current tab is closed.
    // After the current tab is closed, send mouse up to end drag and drop.
    if (browser()->tab_strip_model()->count() == 1) {
      EXPECT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
          ui_controls::LEFT, ui_controls::UP, std::move(quit)));
      return;
    }

    gfx::Rect bounds = browser()
                           ->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetContainerBounds();
    EXPECT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        bounds.CenterPoint().x(), bounds.CenterPoint().y(),
        base::BindOnce(&DragAndDropBrowserTestNoParam::Step3,
                       base::Unretained(this), std::move(quit))));
  }
};

// https://crbug.com/1312505
IN_PROC_BROWSER_TEST_F(DragAndDropBrowserTestNoParam, CloseTabDuringDrag) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ui_test_utils::TabAddedWaiter wait_for_new_tab(browser());

  // Create a new tab that closes itself on dragover event.
  ASSERT_TRUE(ExecJs(browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetPrimaryMainFrame(),
                     "window.open('javascript:document.addEventListener("
                     "\"dragover\", () => {window.close(); })');"));

  wait_for_new_tab.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  base::RunLoop loop;
  SimulateDragFromOmniboxToWebContents(loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace chrome
