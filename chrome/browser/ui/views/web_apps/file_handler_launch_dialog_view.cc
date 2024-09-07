// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/file_handler_launch_dialog_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace web_app {

FileHandlerLaunchDialogView::FileHandlerLaunchDialogView(
    const std::vector<base::FilePath>& file_paths,
    Profile* profile,
    const webapps::AppId& app_id,
    WebAppLaunchAcceptanceCallback close_callback)
    : LaunchAppUserChoiceDialogView(profile, app_id, std::move(close_callback)),
      file_paths_(file_paths) {
  DCHECK(!file_paths.empty());
  auto* layout_provider = views::LayoutProvider::Get();
  gfx::Insets dialog_insets = layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
#if BUILDFLAG(IS_CHROMEOS)
  // The Chrome OS dialog has no title and no need for a top inset.
  dialog_insets.set_top(0);
#endif
  set_margins(dialog_insets);
  set_fixed_width(layout_provider->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_WEB_APP_FILE_HANDLING_POSITIVE_BUTTON));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_WEB_APP_FILE_HANDLING_NEGATIVE_BUTTON));
}

FileHandlerLaunchDialogView::~FileHandlerLaunchDialogView() = default;

std::unique_ptr<views::View>
FileHandlerLaunchDialogView::CreateAboveAppInfoView() {
  // Question label.
  if (file_paths_.size() > 1) {
    auto question_label =
        std::make_unique<views::Label>(l10n_util::GetPluralStringFUTF16(
            IDS_WEB_APP_FILE_HANDLING_DIALOG_QUESTION_MULTIPLE,
            file_paths_.size()));
    question_label->SetMultiLine(true);
    question_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return question_label;
  }

  // Insert and emphasize the file name.
  auto question_label = std::make_unique<views::StyledLabel>();
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = question_label->GetFontList().Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD);
  std::vector<size_t> offsets;
  std::u16string file_name_text = gfx::ElideFilename(
      file_paths_[0].BaseName(), *style.custom_font, fixed_width() * 2 / 3);
  question_label->SetText(l10n_util::GetStringFUTF16(
      IDS_WEB_APP_FILE_HANDLING_DIALOG_QUESTION, {file_name_text}, &offsets));
  DCHECK_EQ(1u, offsets.size());
  question_label->AddStyleRange(
      gfx::Range(offsets[0], offsets[0] + file_name_text.length()), style);
  return question_label;
}

std::unique_ptr<views::View>
FileHandlerLaunchDialogView::CreateBelowAppInfoView() {
  if (file_paths_.size() == 1) {
    return nullptr;
  }

  auto* layout_provider = views::LayoutProvider::Get();
  auto description_view = std::make_unique<views::View>();
  description_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // File icon and name list.
  auto* files_view =
      description_view->AddChildView(std::make_unique<views::View>());
  files_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // TODO(tluk): We should be sourcing the size of the file icon from the
  // layout provider rather than relying on hardcoded constants.
  constexpr int kIconSize = 16;
  auto* icon = files_view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kDescriptionIcon, ui::kColorIcon, kIconSize)));
  const int icon_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  icon->SetProperty(views::kMarginsKey,
                    gfx::Insets::TLBR(0, 0, 0, icon_margin));

  // File name list.
  std::vector<std::u16string> file_names;
  // Display at most a dozen files. After that, elide.
  size_t displayed_file_name_count =
      std::min(file_paths_.size(), static_cast<size_t>(12));
  file_names.reserve(displayed_file_name_count + 1);

  // Additionally, elide very long file names (the max width is the width
  // available for the label).
  const int available_width = fixed_width() - margins().width() -
                              icon->GetPreferredSize().width() - icon_margin;
  std::transform(file_paths_.begin(),
                 file_paths_.begin() + displayed_file_name_count,
                 std::back_inserter(file_names),
                 [available_width](const base::FilePath& file_path) {
                   // Use slightly less than the available width since some
                   // space is needed for the separator.
                   return gfx::ElideFilename(file_path.BaseName(),
                                             views::Label::GetDefaultFontList(),
                                             0.95 * available_width);
                 });
  if (file_paths_.size() > displayed_file_name_count) {
    file_names.emplace_back(gfx::kEllipsisUTF16);
  }

  auto* files_label =
      files_view->AddChildView(std::make_unique<views::Label>(base::JoinString(
          file_names, l10n_util::GetStringUTF16(
                          IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR))));
  files_label->SetMultiLine(true);
  files_label->SetMaximumWidth(available_width);
  files_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  return description_view;
}

std::u16string FileHandlerLaunchDialogView::GetRememberChoiceString() {
  auto [associations, association_count] =
      GetFileTypeAssociationsHandledByWebAppForDisplay(profile(), app_id());
  return base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_WEB_APP_FILE_HANDLING_DIALOG_STICKY_CHOICE),
      "FILE_TYPE_COUNT", static_cast<int>(association_count), "FILE_TYPES",
      associations);
}

BEGIN_METADATA(FileHandlerLaunchDialogView)
END_METADATA

void ShowWebAppFileLaunchDialog(const std::vector<base::FilePath>& file_paths,
                                Profile* profile,
                                const webapps::AppId& app_id,
                                WebAppLaunchAcceptanceCallback close_callback) {
  auto view = std::make_unique<web_app::FileHandlerLaunchDialogView>(
      file_paths, profile, app_id, std::move(close_callback));
  view->Init();
  views::DialogDelegate::CreateDialogWidget(std::move(view),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr)
      ->Show();
}

}  // namespace web_app
