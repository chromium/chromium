// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/drag_and_drop_test_utils.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace drag_and_drop_test_utils {

namespace {
// These are ui::DropTargetEvent::source_operations_ being sent when manually
// trying out drag&drop of an image file from Nemo (Ubuntu's file explorer)
// into a content_shell.
constexpr int kDefaultSourceOperations = ui::DragDropTypes::DRAG_MOVE |
                                         ui::DragDropTypes::DRAG_COPY |
                                         ui::DragDropTypes::DRAG_LINK;
}  // namespace

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* web_contents)
    : DragAndDropSimulator(web_contents, web_contents) {}

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* drag_contents,
                                           content::WebContents* drop_contents)
    : drag_contents_(drag_contents), drop_contents_(drop_contents) {}

DragAndDropSimulator::~DragAndDropSimulator() = default;

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const std::string& text) {
  os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  os_exchange_data_->SetString(base::UTF8ToUTF16(text));
  return SimulateDragEnter(location, std::move(os_exchange_data_));
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const GURL& url) {
  os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  os_exchange_data_->SetURL(url, base::UTF8ToUTF16(url.spec()));
  return SimulateDragEnter(location, std::move(os_exchange_data_));
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const base::FilePath& file) {
  os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  os_exchange_data_->SetFilename(file);
  return SimulateDragEnter(location, std::move(os_exchange_data_));
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    const std::vector<ui::FileInfo>& file_infos) {
  os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  os_exchange_data_->SetFilenames(file_infos);
  return SimulateDragEnter(location, std::move(os_exchange_data_));
}

#if BUILDFLAG(IS_WIN)
bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    const std::vector<std::pair<base::FilePath, base::span<const uint8_t>>>&
        filenames_and_contents,
    DWORD tymed) {
  os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
  os_exchange_data_->provider().SetVirtualFileContentsForTesting(
      filenames_and_contents, tymed);
  return SimulateDragEnter(location, std::move(os_exchange_data_));
}
#endif  // BUILDFLAG(IS_WIN)

bool DragAndDropSimulator::SimulateOmniboxDragEnter(aura::Window* omnibox,
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

  active_drag_event_ = std::make_unique<ui::DropTargetEvent>(
      *os_exchange_data_, gfx::PointF(location), gfx::PointF(location),
      kDefaultSourceOperations);

  delegate->OnDragEntered(*active_drag_event_);
  delegate->OnDragUpdated(*active_drag_event_);
  return true;
}

bool DragAndDropSimulator::SimulateDrop(const gfx::Point& location) {
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

bool DragAndDropSimulator::SimulateOmniboxDrop(aura::Window* omnibox,
                                               const gfx::Point& location) {
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

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    std::unique_ptr<ui::OSExchangeData> data) {
  if (active_drag_event_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }

  aura::client::DragDropDelegate* delegate = GetDragDelegate();
  if (!delegate) {
    return false;
  }

  CHECK(data);

  os_exchange_data_ = std::move(data);

  gfx::PointF event_location;
  gfx::PointF event_root_location;
  CalculateEventLocations(location, &event_location, &event_root_location,
                          drag_contents_);
  active_drag_event_ = std::make_unique<ui::DropTargetEvent>(
      *os_exchange_data_, event_location, event_root_location,
      kDefaultSourceOperations);

  delegate->OnDragEntered(*active_drag_event_);
  delegate->OnDragUpdated(*active_drag_event_);
  return true;
}

aura::client::DragDropDelegate* DragAndDropSimulator::GetDragDelegate() {
  gfx::NativeView view = drag_contents_->GetContentNativeView();
  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(view);
  EXPECT_TRUE(delegate) << "Expecting WebContents to have DragDropDelegate";
  return delegate;
}

aura::client::DragDropDelegate* DragAndDropSimulator::GetDropDelegate() {
  gfx::NativeView view = drop_contents_->GetContentNativeView();
  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(view);
  EXPECT_TRUE(delegate) << "Expecting WebContents to have DragDropDelegate";
  return delegate;
}

aura::client::DragDropDelegate*
DragAndDropSimulator::GetOmniboxDragDropDelegate(aura::Window* omnibox) {
  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(omnibox);
  EXPECT_TRUE(delegate) << "Expecting Omnibox to have DragDropDelegate";
  return delegate;
}

void DragAndDropSimulator::CalculateEventLocations(
    const gfx::Point& web_contents_relative_location,
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

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents)
    : DragStartWaiter(web_contents, base::DoNothing()) {}

DragStartWaiter::DragStartWaiter(content::WebContents* web_contents,
                                 base::OnceClosure on_drag_started_callback)
    : web_contents_(web_contents),
      on_drag_started_callback_(std::move(on_drag_started_callback)) {
  CHECK(web_contents_);
  CHECK(web_contents_->GetContentNativeView());
  gfx::NativeWindow root_window =
      web_contents_->GetContentNativeView()->GetRootWindow();
  CHECK(root_window);
  old_client_ = aura::client::GetDragDropClient(root_window);
  CHECK(old_client_);
  aura::client::SetDragDropClient(root_window, this);
}

DragStartWaiter::~DragStartWaiter() {
  DragCancel();
  if (web_contents_->GetContentNativeView()) {
    gfx::NativeWindow root_window =
        web_contents_->GetContentNativeView()->GetRootWindow();
    if (root_window) {
      aura::client::SetDragDropClient(root_window, old_client_);
    }
  }
}

void DragStartWaiter::WaitUntilDragStart() {
  run_loop_.Run();
}

void DragStartWaiter::ReleaseDrag() {
  release_loop_.Quit();
}

std::unique_ptr<ui::OSExchangeData> DragStartWaiter::TakeCapturedData() {
  CHECK(suppress_passing_further_ || captured_data_)
      << "Cannot extract captured data unless drag is suppressed or completed.";
  return std::move(captured_data_);
}

void DragStartWaiter::SuppressPassingStartDragFurther() {
  suppress_passing_further_ = true;
}

ui::mojom::DragOperation DragStartWaiter::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  CHECK(!on_drag_started_callback_ || suppress_passing_further_)
      << "on_drag_started_callback requires SuppressPassingStartDragFurther "
         "to be called.";
  captured_data_ = std::move(data);
  run_loop_.Quit();

  if (suppress_passing_further_) {
    if (on_drag_started_callback_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_drag_started_callback_));
    }
    release_loop_.Run();
    return ui::mojom::DragOperation::kCopy;
  }

  CHECK(captured_data_);
  return old_client_->StartDragAndDrop(std::move(captured_data_), root_window,
                                       source_window, screen_location,
                                       allowed_operations, source);
}

void DragStartWaiter::DragCancel() {
  release_loop_.Quit();
}

#if BUILDFLAG(IS_LINUX)
void DragStartWaiter::UpdateDragImage(const gfx::ImageSkia& image,
                                      const gfx::Vector2d& offset) {}
#endif

bool DragStartWaiter::IsDragDropInProgress() {
  return captured_data_ != nullptr;
}

void DragStartWaiter::AddObserver(
    aura::client::DragDropClientObserver* observer) {}

void DragStartWaiter::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {}

}  // namespace drag_and_drop_test_utils
