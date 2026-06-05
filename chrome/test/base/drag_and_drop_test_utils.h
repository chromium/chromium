// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_
#define CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

#if BUILDFLAG(IS_WIN)
#include "base/containers/span.h"
#include "base/win/windows_types.h"
#endif

class GURL;

namespace content {
class WebContents;
}

namespace ui {
struct FileInfo;
class DropTargetEvent;
class OSExchangeData;
}  // namespace ui

namespace aura {
class Window;
namespace client {
class DragDropDelegate;
}
}  // namespace aura

namespace drag_and_drop_test_utils {

// Test helper for simulating drag and drop happening in WebContents.
// Acts as the **Drag Target Injector**: programmatically simulates OS-level
// drag events (DragEnter, DragOver, Drop) on the destination WebContents view,
// allowing tests to inject either synthetic mock payloads or real captured data
// directly into the target's event handlers.
//
// Contrast with DragStartWaiter, which operates on the drag source side to
// intercept and capture data.
// Adapted from chrome/browser/ui/views/drag_and_drop_interactive_uitest.cc
class DragAndDropSimulator {
 public:
  // Creates a simulator where both the drag source and drop target are
  // the same `web_contents`.
  explicit DragAndDropSimulator(content::WebContents* web_contents);

  // Creates a simulator where the drag and drop flow spans different
  // WebContents, starting from `drag_contents` and ending at `drop_contents`.
  DragAndDropSimulator(content::WebContents* drag_contents,
                       content::WebContents* drop_contents);

  DragAndDropSimulator(const DragAndDropSimulator&) = delete;
  DragAndDropSimulator& operator=(const DragAndDropSimulator&) = delete;

  // Cleans up the simulator and any pending drag states.
  ~DragAndDropSimulator();

  // Simulates notification that |text| was dragged from outside of the browser,
  // into the specified |location| inside |web_contents|.
  // |location| is relative to |web_contents|.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location, const std::string& text);

  // Simulates notification that |url| was dragged from outside of the browser,
  // into the specified |location| inside |web_contents|.
  // |location| is relative to |web_contents|.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location, const GURL& url);

  // Simulates notification that |file| was dragged from outside of the browser,
  // into the specified |location| inside |web_contents|.
  // |location| is relative to |web_contents|.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location,
                         const base::FilePath& file);

  // Simulates notification that multiple files were dragged from outside of the
  // browser, into the specified `location` inside `web_contents`. `location` is
  // relative to `web_contents`. Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location,
                         const std::vector<ui::FileInfo>& file_infos);

#if BUILDFLAG(IS_WIN)
  // Simulates notification that multiple virtual files were dragged from
  // outside of the browser, into the specified `location` inside
  // `web_contents`. `location` is relative to `web_contents`. Returns true upon
  // success.
  bool SimulateDragEnter(
      const gfx::Point& location,
      const std::vector<std::pair<base::FilePath, base::span<const uint8_t>>>&
          filenames_and_contents,
      DWORD tymed);
#endif  // BUILDFLAG(IS_WIN)

  // Simulates notification that |url| was dragged from outside of the browser,
  // into the specified |location| inside |omnibox|.
  // |location| is relative to |omnibox|.
  // Returns true upon success.
  bool SimulateOmniboxDragEnter(aura::Window* omnibox,
                                const gfx::Point& location,
                                const GURL& url);

  // Simulates dropping of the drag-and-dropped item.
  // SimulateDragEnter needs to be called first.
  // Returns true upon success.
  bool SimulateDrop(const gfx::Point& location);

  // Simulates dropping of the drag-and-dropped item into |omnibox|.
  // SimulateDragEnter needs to be called first.
  // Returns true upon success.
  bool SimulateOmniboxDrop(aura::Window* omnibox, const gfx::Point& location);

  // Simulates notification that an item was dragged from outside of the
  // browser, using the specified `data` into the specified `location` inside
  // `drag_contents_`.
  // `location` is relative to `drag_contents_`.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location,
                         std::unique_ptr<ui::OSExchangeData> data);

 private:
  aura::client::DragDropDelegate* GetDragDelegate();
  aura::client::DragDropDelegate* GetDropDelegate();
  aura::client::DragDropDelegate* GetOmniboxDragDropDelegate(
      aura::Window* omnibox);

  void CalculateEventLocations(const gfx::Point& web_contents_relative_location,
                               gfx::PointF* out_event_location,
                               gfx::PointF* out_event_root_location,
                               content::WebContents* contents);

  // WebContents for where the drag and drop occurs. These can be the same if
  // the drag and drop happens within the same WebContents.
  raw_ptr<content::WebContents> drag_contents_;
  raw_ptr<content::WebContents> drop_contents_;

  std::unique_ptr<ui::DropTargetEvent> active_drag_event_;
  std::unique_ptr<ui::OSExchangeData> os_exchange_data_;
};

// Helper for waiting until a drag-and-drop starts (e.g., in response to a
// mouse-down + mouse-move simulated by the test).
// Acts as the `Drag Source Interceptor`: overrides Aura's window-wide
// DragDropClient to intercept the drag initiation at the source window, capture
// the pristine `OSExchangeData` containing Blink-generated metadata, and
// suppress the native blocking run loop that would otherwise halt the test
// runner thread.
//
// This class must be used exclusively on the UI thread.
//
// Contrast with `DragAndDropSimulator`, which operates on the drag target side
// to inject and simulate events.
class DragStartWaiter : public aura::client::DragDropClient {
 public:
  // Registers this waiter as the temporary Aura DragDropClient on the root
  // window hosting `web_contents`. If `on_drag_started_callback` is provided,
  // it runs once a drag is initiated.
  explicit DragStartWaiter(content::WebContents* web_contents);
  DragStartWaiter(content::WebContents* web_contents,
                  base::OnceClosure on_drag_started_callback);

  DragStartWaiter(const DragStartWaiter&) = delete;
  DragStartWaiter& operator=(const DragStartWaiter&) = delete;

  ~DragStartWaiter() override;

  // Blocks the test runner's execution until a drag-and-drop event is
  // initiated.
  void WaitUntilDragStart();

  // Releases the intercepted drag execution loop, allowing the blocked source's
  // StartDragAndDrop call to return a simulated completion result.
  void ReleaseDrag();

  // Returns the captured OSExchangeData payload, transferring ownership to the
  // caller.
  std::unique_ptr<ui::OSExchangeData> TakeCapturedData();

  // Configures the waiter to intercept and suppress the drag propagation,
  // preventing Aura from passing the drag request down to the window manager.
  void SuppressPassingStartDragFurther();

  // aura::client::DragDropClient:
  ui::mojom::DragOperation StartDragAndDrop(
      std::unique_ptr<ui::OSExchangeData> data,
      aura::Window* root_window,
      aura::Window* source_window,
      const gfx::Point& screen_location,
      int allowed_operations,
      ui::mojom::DragEventSource source) override;
  void DragCancel() override;
#if BUILDFLAG(IS_LINUX)
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override;
#endif
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
  raw_ptr<aura::client::DragDropClient> old_client_ = nullptr;
  base::RunLoop run_loop_;
  base::RunLoop release_loop_;
  base::OnceClosure on_drag_started_callback_;
  std::unique_ptr<ui::OSExchangeData> captured_data_;
  bool suppress_passing_further_ = false;
};

}  // namespace drag_and_drop_test_utils

#endif  // CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_
