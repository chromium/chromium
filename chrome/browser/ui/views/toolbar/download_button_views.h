// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_DOWNLOAD_BUTTON_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_DOWNLOAD_BUTTON_VIEWS_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/toolbar/download_button.h"

class PinnedActionToolbarButton;
class PinnedToolbarActionsContainer;

// Views implementation of DownloadButton. Wraps the downloads button in a
// PinnedToolbarActionsContainer. The button is looked up on every call since
// the container creates and destroys it as it's pinned, unpinned, popped out,
// etc. Callers should check that the container has a downloads button (i.e.
// that PinnedToolbarActionsContainer::GetDownloadButton() returns non-null)
// before calling methods on this.
class DownloadButtonViews : public DownloadButton {
 public:
  explicit DownloadButtonViews(PinnedToolbarActionsContainer& container);
  DownloadButtonViews(const DownloadButtonViews&) = delete;
  DownloadButtonViews& operator=(const DownloadButtonViews&) = delete;
  ~DownloadButtonViews() override;

  // DownloadButton:
  bool IsShowing() const override;
  SkColor GetColor(ui::ColorId color_id) const override;
  void AnnounceAccessibleAlert(const std::u16string& text) override;
  void UpdateProgressRing(ActionItemProgressRingStatus status,
                          int progress_percentage) override;
  void UpdateBadge(bool is_active,
                   int progress_download_count,
                   SkColor text_color,
                   SkColor background_color) override;
  gfx::Rect GetBoundsInScreen() const override;
  void SetElementIdentifier(ui::ElementIdentifier element_id) override;
  ActionItemProgressRingStatus GetProgressRingStatusForTesting() override;
  views::ImageView* GetImageBadgeForTesting() override;

 private:
  // Returns the downloads button. Must only be called when the container has
  // one.
  PinnedActionToolbarButton& GetButton() const;

  const raw_ref<PinnedToolbarActionsContainer> container_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_DOWNLOAD_BUTTON_VIEWS_H_
