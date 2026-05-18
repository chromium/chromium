// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility_annotator/personal_context_notice_dialog.h"

#include <utility>

#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/webview/webview.h"

namespace personal_context::notice {

namespace {
constexpr int kBubbleWidth = 448;
}  // namespace

PersonalContextNoticeDialog::PersonalContextNoticeDialog(
    views::View* anchor_view,
    std::unique_ptr<WebUIContentsWrapperT<PersonalContextNoticeUI>>
        contents_wrapper)
    : WebUIBubbleDialogView(anchor_view,
                            contents_wrapper->GetWeakPtr(),
                            std::nullopt,
                            views::BubbleBorder::FLOAT,
                            /*autosize=*/true),
      contents_wrapper_(std::move(contents_wrapper)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetModalType(ui::mojom::ModalType::kNone);
  set_fixed_width(kBubbleWidth);
  set_margins(gfx::Insets());
}

PersonalContextNoticeDialog::~PersonalContextNoticeDialog() = default;

BEGIN_METADATA(PersonalContextNoticeDialog)
END_METADATA

}  // namespace personal_context::notice
