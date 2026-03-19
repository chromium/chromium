// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_
#define CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

#if BUILDFLAG(IS_WIN)
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
// Adapted from chrome/browser/ui/views/drag_and_drop_interactive_uitest.cc
class DragAndDropSimulator {
 public:
  explicit DragAndDropSimulator(content::WebContents* web_contents);

  DragAndDropSimulator(content::WebContents* drag_contents,
                       content::WebContents* drop_contents);

  DragAndDropSimulator(const DragAndDropSimulator&) = delete;
  DragAndDropSimulator& operator=(const DragAndDropSimulator&) = delete;

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
      const std::vector<std::pair<base::FilePath, std::string>>&
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

 private:
  bool SimulateDragEnter(const gfx::Point& location,
                         const ui::OSExchangeData& data);

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

}  // namespace drag_and_drop_test_utils

#endif  // CHROME_TEST_BASE_DRAG_AND_DROP_TEST_UTILS_H_
