// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_SOURCE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_SOURCE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/view.h"

// View displaying a preview, icon and title for the tab being shared, or a
// throbber while the dialog is not yet activated.
class ShareThisTabSourceView : public views::View {
  METADATA_HEADER(ShareThisTabSourceView, views::View)

 public:
  explicit ShareThisTabSourceView(
      base::WeakPtr<content::WebContents> web_contents);
  ShareThisTabSourceView(const ShareThisTabSourceView&) = delete;
  ShareThisTabSourceView& operator=(const ShareThisTabSourceView&) = delete;
  ~ShareThisTabSourceView() override;

  void Activate();
  void StopRefreshing();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;

 private:
  void UpdateFaviconAndTabTitle();

  void Refresh();

  // Called on the UI thread after the captured image is handled. If the
  // image was new, it's rescaled to the desired size and sent back in |image|.
  // Otherwise, an empty Optional is sent back. In either case, |hash| is the
  // hash value of the frame that was handled.
  void OnCaptureHandled(uint32_t hash,
                        const std::optional<gfx::ImageSkia>& image);

  raw_ptr<views::Throbber> throbber_ = nullptr;
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<views::ImageView> favicon_view_ = nullptr;
  raw_ptr<views::Label> tab_title_label_ = nullptr;

  // The capturing tab's WebContent
  const base::WeakPtr<content::WebContents> web_contents_;

  // The hash of the last captured frame. Used to detect identical frames
  // and prevent needless rescaling.
  std::optional<uint32_t> last_hash_;

  // The heavy lifting involved with rescaling images into thumbnails is
  // moved off of the UI thread and onto this task runner.
  scoped_refptr<base::SequencedTaskRunner> thumbnail_task_runner_;

  // Blocks refreshing when the dialog is closed.
  bool refreshing_ = false;

  base::WeakPtrFactory<ShareThisTabSourceView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_SOURCE_VIEW_H_
