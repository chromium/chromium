// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_
#define CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/point.h"

#if BUILDFLAG(IS_WIN)
#include "base/containers/span.h"
#include "base/win/windows_types.h"
#endif

#if defined(USE_AURA)
namespace aura {
class Window;
}
#endif

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
struct FileInfo;
class OSExchangeData;
}  // namespace ui

namespace drag_and_drop_test_utils {

// Test helper for simulating drag and drop happening in WebContents.
// Acts as the **Drag Target Injector**: programmatically simulates OS-level
// drag events (DragEnter, DragOver, Drop) on the destination WebContents view,
// allowing tests to inject either synthetic mock payloads or real captured data
// directly into the target's event handlers.
//
// This class must be used exclusively on the UI thread.
//
// Contrast with `DragStartWaiter`, which operates on the drag source side to
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

  // Simulates notification that `text` was dragged from outside of the
  // browser, into the specified `location` inside `drag_contents`.
  // `location` is relative to `drag_contents`.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location, const std::string& text);

  // Simulates notification that `url` was dragged from outside of the browser,
  // into the specified `location` inside `drag_contents`.
  // `location` is relative to `drag_contents`.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location, const GURL& url);

  // Simulates notification that `file` was dragged from outside of the browser,
  // into the specified `location` inside `drag_contents`.
  // `location` is relative to `drag_contents`.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location,
                         const base::FilePath& file);

  // Simulates notification that multiple files were dragged from outside of
  // the browser, into the specified `location` inside `drag_contents`.
  // `location` is relative to `drag_contents`.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location,
                         const std::vector<ui::FileInfo>& file_infos);

  // Simulates notification that an item was dragged from outside of the
  // browser, using the specified `data` into the specified `location` inside
  // `drag_contents`.
  // `location` is relative to `drag_contents`.
  // Returns true upon success.
  bool SimulateDragEnter(const gfx::Point& location,
                         std::unique_ptr<ui::OSExchangeData> data);

#if BUILDFLAG(IS_WIN)
  // Simulates notification that multiple virtual files were dragged from
  // outside of the browser, into the specified `location` inside
  // `drag_contents`.
  // `location` is relative to `drag_contents`.
  // Returns true upon success.
  bool SimulateDragEnter(
      const gfx::Point& location,
      const std::vector<std::pair<base::FilePath, base::span<const uint8_t>>>&
          filenames_and_contents,
      DWORD tymed);
#endif  // BUILDFLAG(IS_WIN)

#if defined(USE_AURA)
  // Simulates notification that `url` was dragged from outside of the browser,
  // into the specified `location` inside `omnibox`.
  // `location` is relative to `omnibox`.
  // Returns true upon success.
  bool SimulateOmniboxDragEnter(aura::Window* omnibox,
                                const gfx::Point& location,
                                const GURL& url);

  // Simulates dropping of the drag-and-dropped item into `omnibox`.
  // `SimulateDragEnter` needs to be called first.
  // Returns true upon success.
  bool SimulateOmniboxDrop(aura::Window* omnibox, const gfx::Point& location);
#endif

  // Simulates dropping of the drag-and-dropped item.
  // `SimulateDragEnter` needs to be called first.
  // Returns true upon success.
  bool SimulateDrop(const gfx::Point& location);

 private:
  struct PlatformState;

  std::unique_ptr<PlatformState> state_;
};

// Helper for waiting until a drag-and-drop starts (e.g., in response to a
// mouse-down + mouse-move simulated by the test).
// Acts as the `Drag Source Interceptor`: overrides platform drag handlers to
// intercept the drag initiation at the source window, capture the pristine
// `OSExchangeData` containing Blink-generated metadata, and suppress the
// native blocking run loop that would otherwise halt the test runner thread.
//
// This class must be used exclusively on the UI thread.
//
// Contrast with `DragAndDropSimulator`, which operates on the drag target side
// to inject and simulate events.
class DragStartWaiter {
 public:
  // Registers this waiter on `web_contents`. If `on_drag_started_callback`
  // is provided, it runs once a drag is initiated.
  explicit DragStartWaiter(content::WebContents* web_contents);
  DragStartWaiter(content::WebContents* web_contents,
                  base::OnceClosure on_drag_started_callback);

  DragStartWaiter(const DragStartWaiter&) = delete;
  DragStartWaiter& operator=(const DragStartWaiter&) = delete;

  ~DragStartWaiter();

  // Blocks the test runner's execution until a drag-and-drop event is
  // initiated.
  void WaitUntilDragStart();

  // Releases the suppressed drag loop after capturing `OSExchangeData`.
  void ReleaseDrag();

  // Extracts the captured `OSExchangeData` received from Blink when the drag
  // was initiated.
  std::unique_ptr<ui::OSExchangeData> TakeCapturedData();

  // Configures the waiter to intercept and suppress the drag propagation,
  // preventing the OS from passing the drag request down to the window manager.
  void SuppressPassingStartDragFurther();

 private:
  struct PlatformState;

  std::unique_ptr<PlatformState> state_;
};

}  // namespace drag_and_drop_test_utils

#endif  // CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_
