// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/drag_and_drop_test_utils.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/android/jar_jni/DragEvent_jni.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace drag_and_drop_test_utils {

namespace {

// Walks up the ViewAndroid hierarchy starting at `view` to find the
// `ui::EventForwarder` attached to the root ContentView, which routes drag
// events back down into `target_contents`'s `WebContentsViewAndroid`.
ui::EventForwarder* FindEventForwarder(ui::ViewAndroid* view) {
  for (ui::ViewAndroid* curr = view; curr; curr = curr->parent()) {
    if (ui::EventForwarder* forwarder = curr->event_forwarder()) {
      return forwarder;
    }
  }
  return nullptr;
}

// Returns the `ui::EventForwarder` that routes drag events into
// `web_contents`, creating one on its view if none exists yet. Populates
// `location_px` (view-relative) and `screen_location_px` (screen-space) in
// physical pixels from `location` (DIPs, relative to `web_contents`).
ui::EventForwarder* GetEventForwarderForLocation(
    content::WebContents* web_contents,
    const gfx::Point& location,
    gfx::PointF* location_px,
    gfx::PointF* screen_location_px) {
  if (!web_contents) {
    return nullptr;
  }
  ui::ViewAndroid* view = web_contents->GetContentNativeView();
  if (!view) {
    return nullptr;
  }
  float dip_scale = view->GetDipScale();
  *location_px =
      gfx::PointF(location.x() * dip_scale, location.y() * dip_scale);
  *screen_location_px =
      view->GetLocationOnScreen(location_px->x(), location_px->y());

  ui::EventForwarder* forwarder = FindEventForwarder(view);
  if (!forwarder) {
    // `GetEventForwarder()` lazily initializes `view->event_forwarder_` and
    // its Java `EventForwarder` peer if none exists yet on the ViewAndroid
    // hierarchy.
    view->GetEventForwarder();
    forwarder = view->event_forwarder();
  }
  return forwarder;
}

// Drains any in-flight asynchronous hit-test requests queued in
// `RenderWidgetTargeter` (such as `WebContentsViewAndroid::OnDragUpdated` or
// `OnPerformDrop` querying an inner `<webview>` via `FrameSinkIdAt`).
//
// `content::SimulateMouseEvent` routes a `kMouseMove` through the same FIFO
// `RenderWidgetTargeter::requests_` queue as
// `GetRenderWidgetHostAtPointAsynchronously`, so `RenderWidgetTargeter` cannot
// dispatch `kMouseMove` until all preceding drag hit-test callbacks have run on
// the browser UI thread.
void FlushPendingHitTests(content::WebContents* web_contents,
                          const gfx::Point& location) {
  if (!web_contents) {
    return;
  }
  content::WebContents* outermost = web_contents->GetOutermostWebContents();
  if (!outermost || !outermost->GetRenderWidgetHostView()) {
    return;
  }

  class MouseMoveDispatchWaiter
      : public content::RenderWidgetHost::InputEventObserver,
        public content::RenderWidgetHostObserver {
   public:
    explicit MouseMoveDispatchWaiter(
        const std::vector<content::RenderWidgetHostView*>& views) {
      for (content::RenderWidgetHostView* view : views) {
        if (!view || !view->GetRenderWidgetHost()) {
          continue;
        }
        content::RenderWidgetHost* host = view->GetRenderWidgetHost();
        if (hosts_.insert(host).second) {
          host->AddInputEventObserver(this);
          host_observations_.AddObservation(host);
        }
      }
    }

    ~MouseMoveDispatchWaiter() override {
      for (content::RenderWidgetHost* host : hosts_) {
        host->RemoveInputEventObserver(this);
      }
    }

    bool has_hosts() const { return !hosts_.empty(); }

    void Wait() {
      if (!dispatched_ && !hosts_.empty()) {
        run_loop_.Run();
      }
    }

    // content::RenderWidgetHost::InputEventObserver:
    void OnInputEvent(const content::RenderWidgetHost& widget,
                      const blink::WebInputEvent& event,
                      InputEventSource source) override {
      if (event.GetType() == blink::WebInputEvent::Type::kMouseMove) {
        dispatched_ = true;
        run_loop_.Quit();
      }
    }

    // content::RenderWidgetHostObserver:
    void RenderWidgetHostDestroyed(
        content::RenderWidgetHost* widget_host) override {
      widget_host->RemoveInputEventObserver(this);
      host_observations_.RemoveObservation(widget_host);
      hosts_.erase(widget_host);
      if (hosts_.empty()) {
        run_loop_.Quit();
      }
    }

   private:
    std::set<raw_ptr<content::RenderWidgetHost>> hosts_;
    base::ScopedMultiSourceObservation<content::RenderWidgetHost,
                                       content::RenderWidgetHostObserver>
        host_observations_{this};
    bool dispatched_ = false;
    base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  };

  MouseMoveDispatchWaiter waiter(
      content::GetInputEventRouterRenderWidgetHostViews(outermost));
  if (!waiter.has_hosts()) {
    return;
  }
  content::SimulateMouseEvent(outermost, blink::WebInputEvent::Type::kMouseMove,
                              blink::WebMouseEvent::Button::kNoButton,
                              location);
  waiter.Wait();
}

}  // namespace

CapturedDragData::CapturedDragData() = default;
CapturedDragData::~CapturedDragData() = default;

struct DragAndDropSimulator::PlatformState {
  PlatformState(content::WebContents* drag_contents,
                content::WebContents* drop_contents)
      : drag_contents_(drag_contents), drop_contents_(drop_contents) {}

  bool HasActiveDrag() const {
    return active_drop_data_ || active_captured_data_;
  }

  void ResetActiveDrag() {
    active_drop_data_.reset();
    active_captured_data_.reset();
  }

  bool SendDragEvent(content::WebContents* target_contents,
                     int action,
                     const gfx::Point& location) {
    gfx::PointF location_px;
    gfx::PointF screen_px;
    ui::EventForwarder* forwarder = GetEventForwarderForLocation(
        target_contents, location, &location_px, &screen_px);
    if (!forwarder) {
      return false;
    }

    if (active_captured_data_) {
      forwarder->DispatchDragEventForTesting(
          action, location_px.x(), location_px.y(), screen_px.x(),
          screen_px.y(), active_captured_data_->clip_data);
      return true;
    }

    JNIEnv* env = base::android::AttachCurrentThread();

    base::android::ScopedJavaLocalRef<jobjectArray> j_mime_types;
    base::android::ScopedJavaLocalRef<jstring> j_text;
    base::android::ScopedJavaLocalRef<jstring> j_html;
    base::android::ScopedJavaLocalRef<jstring> j_url;
    base::android::ScopedJavaLocalRef<jobjectArray> j_filenames;
    base::android::ScopedJavaLocalRef<jstring> j_custom_data;
    base::android::ScopedJavaLocalRef<jstring> j_effect_allowed;
    bool has_files = false;

    if (active_drop_data_) {
      const bool is_drop = (action == DragEventJni::ACTION_DROP);
      std::vector<std::u16string> mime_types;
      if (active_drop_data_->text) {
        mime_types.push_back(ui::kMimeTypePlainText16);
        if (is_drop) {
          j_text = base::android::ConvertUTF16ToJavaString(
              env, *active_drop_data_->text);
        }
      }
      if (!active_drop_data_->url_infos.empty()) {
        mime_types.push_back(ui::kMimeTypeMozillaUrl16);
        if (is_drop) {
          j_url = base::android::ConvertUTF8ToJavaString(
              env, active_drop_data_->url_infos.front().url.spec());
        }
      }
      if (!active_drop_data_->filenames.empty()) {
        has_files = true;
        base::ScopedAllowBlockingForTesting allow_blocking;
        std::vector<std::vector<std::string>> file_matrix;
        for (const auto& file_info : active_drop_data_->filenames) {
          std::string mime_type = ui::kMimeTypeOctetStream;
          net::GetMimeTypeFromFile(file_info.path, &mime_type);
          mime_types.push_back(base::UTF8ToUTF16(mime_type));
          if (is_drop) {
            file_matrix.push_back(
                {file_info.path.value(), file_info.display_name.value()});
          }
        }
        if (is_drop) {
          j_filenames =
              base::android::ToJavaArrayOfStringArray(env, file_matrix);
        }
      }
      j_mime_types = base::android::ToJavaArrayOfStrings(env, mime_types);
    }

    forwarder->OnDragEvent(
        env, action, location_px.x(), location_px.y(), screen_px.x(),
        screen_px.y(), j_mime_types, has_files, j_filenames, j_text, j_html,
        j_url, j_custom_data, j_effect_allowed,
        /*j_dragId=*/base::android::JavaRef<jstring>(),
        /*j_imageBytes=*/base::android::JavaRef<jbyteArray>(),
        /*j_imageExtension=*/base::android::JavaRef<jstring>(),
        /*j_imageFilename=*/base::android::JavaRef<jstring>());
    return true;
  }

  bool SendDragEnterEvents(const gfx::Point& location) {
    if (!SendDragEvent(drag_contents_, DragEventJni::ACTION_DRAG_ENTERED,
                       location) ||
        !SendDragEvent(drag_contents_, DragEventJni::ACTION_DRAG_LOCATION,
                       location)) {
      return false;
    }
    FlushPendingHitTests(drag_contents_, location);
    return true;
  }

  raw_ptr<content::WebContents> drag_contents_ = nullptr;
  raw_ptr<content::WebContents> drop_contents_ = nullptr;
  std::unique_ptr<content::DropData> active_drop_data_;
  std::unique_ptr<CapturedDragData> active_captured_data_;
};

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* web_contents)
    : DragAndDropSimulator(web_contents, web_contents) {}

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* drag_contents,
                                           content::WebContents* drop_contents)
    : state_(std::make_unique<PlatformState>(drag_contents, drop_contents)) {}

DragAndDropSimulator::~DragAndDropSimulator() = default;

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const std::string& text) {
  if (state_->HasActiveDrag()) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  state_->active_drop_data_->text = base::UTF8ToUTF16(text);
  return state_->SendDragEnterEvents(location);
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const GURL& url) {
  if (state_->HasActiveDrag()) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  state_->active_drop_data_->url_infos.emplace_back(
      url, base::UTF8ToUTF16(url.spec()));
  state_->active_drop_data_->text = base::UTF8ToUTF16(url.spec());
  return state_->SendDragEnterEvents(location);
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const base::FilePath& file) {
  if (state_->HasActiveDrag()) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  state_->active_drop_data_->filenames.emplace_back(file, file.BaseName());
  return state_->SendDragEnterEvents(location);
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    const std::vector<ui::FileInfo>& file_infos) {
  if (state_->HasActiveDrag()) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  for (const auto& file_info : file_infos) {
    state_->active_drop_data_->filenames.emplace_back(file_info.path,
                                                      file_info.display_name);
  }
  return state_->SendDragEnterEvents(location);
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    std::unique_ptr<CapturedDragData> data) {
  if (state_->HasActiveDrag()) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  if (!data) {
    ADD_FAILURE() << "No drag data was captured for the drag.";
    return false;
  }
  // `data->clip_data` may be null: `DragAndDropDelegateImpl#buildClipData`
  // returns null for DOM element drags without text, link, image or custom
  // data, and `EventForwarder#dispatchDragEventForTesting` accepts that.
  state_->active_captured_data_ = std::move(data);
  return state_->SendDragEnterEvents(location);
}

bool DragAndDropSimulator::SimulateDrop(const gfx::Point& location) {
  if (!state_->HasActiveDrag()) {
    ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
    return false;
  }

  // TODO(crbug.com/508693696): Support dropping into a different WebContents
  // than the one the drag entered. Besides `ACTION_DRAG_ENTERED` on
  // `drop_contents_`, this needs `ACTION_DRAG_EXITED` on `drag_contents_`, a
  // wait for `drop_contents_`'s renderer to acknowledge `dragover` (so
  // `document_is_handling_drag` is set before `ACTION_DROP`), and care when
  // both share one outermost WebContents (e.g. a <webview> guest), whose
  // `WebContentsViewAndroid` would otherwise see a second
  // `ACTION_DRAG_ENTERED`.
  if (state_->drag_contents_ != state_->drop_contents_) {
    ADD_FAILURE() << "Dropping into a different WebContents than the one the "
                     "drag entered is not supported on Android.";
    state_->ResetActiveDrag();
    return false;
  }

  // Refresh drag location at the target coordinates immediately prior to
  // dropping.
  if (!state_->SendDragEvent(state_->drop_contents_,
                             DragEventJni::ACTION_DRAG_LOCATION, location)) {
    state_->ResetActiveDrag();
    return false;
  }

  if (!state_->SendDragEvent(state_->drop_contents_, DragEventJni::ACTION_DROP,
                             location)) {
    state_->ResetActiveDrag();
    return false;
  }
  // Ensure `WebContentsViewAndroid::OnDragUpdated` and `OnPerformDrop`'s
  // asynchronous Viz hit-tests complete before `SimulateDrop()` sends
  // `ACTION_DRAG_ENDED` and returns.
  FlushPendingHitTests(state_->drop_contents_, location);

  bool success = state_->SendDragEvent(
      state_->drop_contents_, DragEventJni::ACTION_DRAG_ENDED, location);
  state_->ResetActiveDrag();
  return success;
}

// Receives drag-start notifications from the Java `DragStartWaiterBridge`,
// which observes drags that `DragAndDropDelegateImpl` starts from
// `web_contents`'s container view while it exists.
class DragStartWaiterAndroid {
 public:
  DragStartWaiterAndroid(content::WebContents* web_contents,
                         base::OnceClosure on_drag_started_callback);

  DragStartWaiterAndroid(const DragStartWaiterAndroid&) = delete;
  DragStartWaiterAndroid& operator=(const DragStartWaiterAndroid&) = delete;

  ~DragStartWaiterAndroid();

  // Called by `DragStartWaiterBridge` with the `ClipData` built for the drag.
  void OnDragStarted(JNIEnv* env,
                     const base::android::JavaRef<jobject>& clip_data) {
    CHECK(suppress_ || !on_drag_started_callback_);
    drag_started_ = true;
    captured_data_ = std::make_unique<CapturedDragData>();
    captured_data_->clip_data.Reset(env, clip_data);
    // Run the callback outside the Java `startDragAndDrop` call stack, and
    // only return from `WaitUntilDragStart()` once it has run.
    if (on_drag_started_callback_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          std::move(on_drag_started_callback_).Then(run_loop_.QuitClosure()));
    } else {
      run_loop_.Quit();
    }
  }

  void WaitUntilDragStart() { run_loop_.Run(); }

  // A suppressed drag never reaches the system, so nothing will deliver
  // `ACTION_DRAG_ENDED` to the source or clear `DropDataProvider`'s cached
  // image. Perform both teardown steps here so `WebContentsViewAndroid` ends
  // the drag in the source renderer and `DropDataProvider` clears its cache.
  void ReleaseDrag();

  std::unique_ptr<CapturedDragData> TakeCapturedData() {
    CHECK(drag_started_);
    return std::move(captured_data_);
  }

  void SuppressPassingStartDragFurther();

 private:
  base::WeakPtr<content::WebContents> web_contents_;
  base::OnceClosure on_drag_started_callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  std::unique_ptr<CapturedDragData> captured_data_;
  bool suppress_ = false;
  bool drag_started_ = false;
  bool released_ = false;
};

}  // namespace drag_and_drop_test_utils

// The generated JNI code needs the complete `DragStartWaiterAndroid` type.
#include "chrome/test/drag_and_drop_test_support_jni_headers/DragStartWaiterBridge_jni.h"

namespace drag_and_drop_test_utils {

DragStartWaiterAndroid::DragStartWaiterAndroid(
    content::WebContents* web_contents,
    base::OnceClosure on_drag_started_callback)
    : on_drag_started_callback_(std::move(on_drag_started_callback)) {
  CHECK(web_contents);
  web_contents_ = web_contents->GetWeakPtr();
  ui::ViewAndroid* view = web_contents->GetNativeView();
  CHECK(view);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> container_view =
      view->GetContainerView();
  CHECK(container_view) << "WebContents has no container view to drag from.";
  java_bridge_.Reset(Java_DragStartWaiterBridge_create(
      env, reinterpret_cast<intptr_t>(this), container_view));
}

DragStartWaiterAndroid::~DragStartWaiterAndroid() {
  // End a suppressed drag that the test did not release, so the source
  // renderer and `DropDataProvider` do not keep stale drag state.
  ReleaseDrag();
  Java_DragStartWaiterBridge_destroy(base::android::AttachCurrentThread(),
                                     java_bridge_);
}

void DragStartWaiterAndroid::ReleaseDrag() {
  if (!suppress_ || !drag_started_ || released_) {
    return;
  }
  released_ = true;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DragStartWaiterBridge_endSuppressedDrag(env, java_bridge_);
  if (!web_contents_) {
    return;
  }
  gfx::PointF location_px;
  gfx::PointF screen_px;
  ui::EventForwarder* forwarder = GetEventForwarderForLocation(
      web_contents_.get(), gfx::Point(1, 1), &location_px, &screen_px);
  ASSERT_TRUE(forwarder);
  forwarder->OnDragEvent(env, DragEventJni::ACTION_DRAG_ENDED, location_px.x(),
                         location_px.y(), screen_px.x(), screen_px.y(), nullptr,
                         false, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr, nullptr, nullptr);
}

void DragStartWaiterAndroid::SuppressPassingStartDragFurther() {
  suppress_ = true;
  Java_DragStartWaiterBridge_setSuppressSystemDrag(
      base::android::AttachCurrentThread(), java_bridge_, true);
}

struct DragStartWaiter::PlatformState : public DragStartWaiterAndroid {
  using DragStartWaiterAndroid::DragStartWaiterAndroid;
};

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents)
    : DragStartWaiter(web_contents, base::OnceClosure()) {}

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents,
                                 base::OnceClosure on_drag_started_callback)
    : state_(std::make_unique<PlatformState>(
          web_contents,
          std::move(on_drag_started_callback))) {}

DragStartWaiter::~DragStartWaiter() = default;

void DragStartWaiter::WaitUntilDragStart() {
  state_->WaitUntilDragStart();
}

void DragStartWaiter::ReleaseDrag() {
  state_->ReleaseDrag();
}

std::unique_ptr<CapturedDragData> DragStartWaiter::TakeCapturedData() {
  return state_->TakeCapturedData();
}

void DragStartWaiter::SuppressPassingStartDragFurther() {
  state_->SuppressPassingStartDragFurther();
}

}  // namespace drag_and_drop_test_utils

DEFINE_JNI(DragStartWaiterBridge)
