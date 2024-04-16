// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace {

std::u16string NormalizeSuggestedTitle(std::u16string title) {
  if (base::StartsWith(title, u"https://")) {
    title = title.substr(8);
  }
  if (base::StartsWith(title, u"http://")) {
    title = title.substr(7);
  }
  return title;
}

std::u16string GetTrimmedTitle(std::u16string title) {
  base::TrimWhitespace(title, base::TRIM_ALL, &title);
  return title;
}

}  // namespace

SiteIconTextAndOriginView::SiteIconTextAndOriginView(
    const gfx::ImageSkia& icon,
    std::u16string initial_title,
    std::u16string accessible_title,
    const GURL& url,
    content::WebContents* web_contents,
    base::RepeatingCallback<void(const std::u16string&)> text_tracker_callback)
    : web_contents_(web_contents),
      text_tracker_callback_(std::move(text_tracker_callback)) {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  const int textfield_width = 320;
  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, textfield_width, 0)
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(views::TableLayout::kFixedSize,
                     layout_provider->GetDistanceMetric(
                         views::DISTANCE_RELATED_CONTROL_VERTICAL))
      .AddRows(1, views::TableLayout::kFixedSize);

  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromImageSkia(icon));
  AddChildView(icon_view.release());

  std::u16string current_title =
      NormalizeSuggestedTitle(GetTrimmedTitle(initial_title));
  text_tracker_callback_.Run(current_title);

  // TODO(dibyapal): Update the SetAccessibleName() to use the correct one for
  // Create Shortcuts. Maybe pass that as an input.
  AddChildView(views::Builder<views::Textfield>()
                   .CopyAddressTo(&title_field_)
                   .SetText(current_title)
                   .SetAccessibleName(accessible_title)
                   .SetController(this)
                   .Build());

  // Skip the first column in the 2nd row, that is the area below the icon and
  // should stay empty.
  AddChildView(views::Builder<views::View>().Build());

  // TODO(dibyapal): Modify to support full urls for Create Shortcut dialog.
  AddChildView(
      web_app::CreateOriginLabelFromStartUrl(url, /*is_primary_text=*/false)
          .release());
  title_field_->SelectAll(true);
}

SiteIconTextAndOriginView::~SiteIconTextAndOriginView() = default;

void SiteIconTextAndOriginView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  CHECK_EQ(sender, title_field_);
  text_tracker_callback_.Run(GetTrimmedTitle(new_contents));

  // TODO(crbug.com/328588659): This shouldn't be needed but we need to undo
  // any position changes that are currently incorrectly caused by a
  // SizeToContents() call, leading to the dialog being anchored off screen
  // from the Chrome window.
  auto* const modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  if (!modal_dialog_manager) {
    return;
  }

  if (!modal_dialog_manager->delegate()) {
    return;
  }

  auto* const modal_dialog_host =
      modal_dialog_manager->delegate()->GetWebContentsModalDialogHost();
  if (!modal_dialog_host) {
    return;
  }

  constrained_window::UpdateWebContentsModalDialogPosition(GetWidget(),
                                                           modal_dialog_host);
}

BEGIN_METADATA(SiteIconTextAndOriginView)
END_METADATA
