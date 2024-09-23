// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_COORDINATOR_H_

#include <optional>
#include <string_view>

#include "ui/gfx/geometry/rect.h"

class WebUIContentsWrapper;
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

  void LoadConsentUI(Profile* profile);
  void LoadEditorUI(Profile* profile,
                    MakoEditorMode mode,
                    bool can_fallback_to_center_position,
                    bool feedback_enabled,
                    std::optional<std::string_view> preset_query_id,
                    std::optional<std::string_view> freeform_text);
  void ShowUI();
  void CloseUI();

  bool IsShowingUI() const;

  // Caches the caret bounds of the current text input client (if one is
  // active). This should be called before showing the mako UI, while the text
  // context for the mako UI is focused.
  void CacheContextCaretBounds();

  gfx::Rect context_caret_bounds_for_testing() const {
    return context_caret_bounds_;
  }

 private:
  // Cached context caret bounds at which to anchor the mako UI. This might not
  // correspond to the most recent active text input client's caret bounds, e.g.
  // if the mako UI was triggered from a freeform text input, the cached caret
  // bounds should correspond to the original text context rather than the
  // freeform input text bounds.
  gfx::Rect context_caret_bounds_;

  // TODO(b/300554470): This doesn't seem like the right class to own the
  // contents wrapper and probably won't handle the bubble widget lifetimes
  // correctly. Figure out how WebUI bubbles work, then implement this properly
  // (maybe using a WebUIBubbleManager).
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_BUBBLE_COORDINATOR_H_
