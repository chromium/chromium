// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_COORDINATOR_H_

#include <string_view>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

class BubbleContentsWrapper;
class Profile;

namespace ash {

enum class MakoEditorMode {
  kWrite,
  kRewrite,
};

// Class used to manage the state of Mako WebUI bubble contents.
class MakoBubbleCoordinator {
 public:
  MakoBubbleCoordinator();
  MakoBubbleCoordinator(const MakoBubbleCoordinator&) = delete;
  MakoBubbleCoordinator& operator=(const MakoBubbleCoordinator&) = delete;
  ~MakoBubbleCoordinator();

  void ShowConsentUI(Profile* profile);
  void ShowEditorUI(Profile* profile,
                    MakoEditorMode mode,
                    absl::optional<std::string_view> preset_query_id,
                    absl::optional<std::string_view> freeform_text);
  void CloseUI();

  bool IsShowingUI() const;

 private:
  // Cached caret bounds to use as the mako UI anchor when there is no text
  // input client (e.g. if focus is not regained after switching from the
  // consent UI to the rewrite UI).
  absl::optional<gfx::Rect> caret_bounds_;

  // TODO(b/300554470): This doesn't seem like the right class to own the
  // contents wrapper and probably won't handle the bubble widget lifetimes
  // correctly. Figure out how WebUI bubbles work, then implement this properly
  // (maybe using a WebUIBubbleManager).
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_COORDINATOR_H_
