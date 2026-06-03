// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"

#include <vector>

#include "base/i18n/message_formatter.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_views_helpers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Returns the message Id to use as heading text, depending on what types of
// usage are present (i.e. just writable files, or also readable directories,
// etc).
// |need_lifetime_text_at_end| is set to false iff the returned message Id
// already includes an explanation for how long a website will have access to
// the listed paths. It is set to true iff a separate label is needed at the end
// of the dialog to explain lifetime.
int ComputeHeadingMessageFromUsage(
    const FileSystemAccessUsageBubbleView::Usage& usage,
    base::FilePath* embedded_path) {
  // Only files.
  if (usage.writable_directories.empty() &&
      usage.readable_directories.empty()) {
    // Only writable files.
    if (usage.readable_files.empty()) {
      DCHECK(!usage.writable_files.empty());
      if (usage.writable_files.size() == 1) {
        *embedded_path = usage.writable_files.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_WRITABLE_FILE_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_WRITABLE_FILES_TEXT;
    }

    // Only readable files.
    if (usage.writable_files.empty()) {
      DCHECK(!usage.readable_files.empty());
      if (usage.readable_files.size() == 1) {
        *embedded_path = usage.readable_files.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_READABLE_FILE_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READABLE_FILES_TEXT;
    }
  }

  // Only directories.
  if (usage.writable_files.empty() && usage.readable_files.empty()) {
    // Only writable directories.
    if (usage.readable_directories.empty()) {
      DCHECK(!usage.writable_directories.empty());
      if (usage.writable_directories.size() == 1) {
        *embedded_path = usage.writable_directories.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_WRITABLE_DIRECTORY_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_WRITABLE_DIRECTORIES_TEXT;
    }

    // Only readable directories.
    if (usage.writable_directories.empty()) {
      DCHECK(!usage.readable_directories.empty());
      if (usage.readable_directories.size() == 1) {
        *embedded_path = usage.readable_directories.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_READABLE_DIRECTORY_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READABLE_DIRECTORIES_TEXT;
    }
  }

  // Only readable files and directories.
  if (usage.writable_files.empty() && usage.writable_directories.empty()) {
    DCHECK(!usage.readable_files.empty());
    DCHECK(!usage.readable_directories.empty());
    return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READABLE_FILES_AND_DIRECTORIES_TEXT;
  }

  // Only writable files and directories.
  if (usage.readable_files.empty() && usage.readable_directories.empty()) {
    DCHECK(!usage.writable_files.empty());
    DCHECK(!usage.writable_directories.empty());
    return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_WRITABLE_FILES_AND_DIRECTORIES_TEXT;
  }

  // Some combination of read and/or write access to files and/or directories.
  return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READ_AND_WRITE;
}

// Displays a (one-column) table model as a one-line summary showing the
// first few items, with a toggle button to expand a table below to contain the
// full list of items.
//
// Visual representation (Collapsed - unelided text fits):
//   +----------------------------------------------------------+
//   | [Icon] file_name_1.txt, file_name_2.png, and 1 more  [v] |
//   +----------------------------------------------------------+
//
// Visual representation (Collapsed - with middle elision fallback):
//   +--------------------------------------------------------+
//   | [Icon] file_..._1.txt, file_..._2.png, and 1 more  [v] |
//   +--------------------------------------------------------+
//
// Visual representation (Expanded):
//   +----------------------------------------------------------+
//   | [Icon] file_name_1.txt, file_name_2.png, and 1 more  [^] |
//   |                                                          |
//   |   +--------------------------------------------------+   |
//   |   | [Icon] file_name_1.txt                           |   |
//   |   | [Icon] file_name_2.png                           |   |
//   |   | [Icon] folder_name                               |   |
//   |   +--------------------------------------------------+   |
//   +----------------------------------------------------------+
class CollapsibleListView : public views::View {
  METADATA_HEADER(CollapsibleListView, views::View)

 public:
  // How many rows to show in the expanded table without having to scroll.
  static constexpr int kExpandedTableRowCount = 3;

  explicit CollapsibleListView(
      FileSystemAccessUsageBubbleView::FilePathListModel* model) {
    const views::LayoutProvider* provider = ChromeLayoutProvider::Get();

    CHECK_GT(model->RowCount(), 0u);

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(0),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    auto summary_row_view = std::make_unique<views::View>();
    int row_horizontal_padding =
        provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
    int between_child_spacing =
        provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    auto* row_layout =
        summary_row_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets::VH(0, row_horizontal_padding), between_child_spacing));

    std::u16string tooltip_text;

    auto icon = std::make_unique<views::ImageView>();
    icon->SetImage(model->GetIcon(0));
    int icon_width = icon->GetPreferredSize().width();

    auto button = views::CreateVectorToggleImageButton(base::BindRepeating(
        &CollapsibleListView::ButtonPressed, base::Unretained(this)));
    button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_ACCESS_USAGE_EXPAND));
    button->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_ACCESS_USAGE_COLLAPSE));
    views::SetImageFromVectorIconWithColor(
        button.get(),
        features::IsRoundedIconsEnabled() ? vector_icons::kKeyboardArrowDownIcon
                                          : vector_icons::kCaretDownOldIcon,
        ui::TableModel::kIconSize, {ui::kColorIcon, ui::kColorIconDisabled});
    views::SetToggledImageFromVectorIconWithColor(
        button.get(),
        features::IsRoundedIconsEnabled() ? vector_icons::kKeyboardArrowUpIcon
                                          : vector_icons::kCaretUpOldIcon,
        ui::TableModel::kIconSize, {ui::kColorIcon, ui::kColorIconDisabled});

    int button_width = button->GetPreferredSize().width();
    bool show_button = model->RowCount() >= 3;
    if (!show_button) {
      button->SetVisible(false);
    }

    int bubble_width =
        provider->GetDistanceMetric(views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

    gfx::Insets dialog_insets =
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);

    // Width consumed by the non-text elements in the row:
    // - Parent dialog horizontal insets
    // - Left and right horizontal padding: 2 * row_horizontal_padding
    // - Icon width
    // - Spacing between icon and label
    int non_text_width = dialog_insets.width() + 2 * row_horizontal_padding +
                         icon_width + between_child_spacing;

    if (show_button) {
      // - Button width
      // - Spacing between label and button
      non_text_width += button_width + between_child_spacing;
    }

    int available_text_width = bubble_width - non_text_width;

    auto label = std::make_unique<views::Label>(
        u"", CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetElideBehavior(gfx::NO_ELIDE);

    const gfx::FontList& font_list = label->font_list();

    // Get unelided names for the first two displayed items.
    base::FilePath first_displayed_path = model->GetPath(0);
    std::u16string first_displayed_name_unelided =
        first_displayed_path.BaseName().LossyDisplayName();

    std::u16string second_displayed_name_unelided;
    base::FilePath second_displayed_path;
    if (model->RowCount() > 1) {
      second_displayed_path = model->GetPath(1);
      second_displayed_name_unelided =
          second_displayed_path.BaseName().LossyDisplayName();
    }

    // Construct the unelided label text to check if it fits.
    std::u16string label_text_unelided =
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_FILES_TEXT),
            base::checked_cast<int64_t>(model->RowCount()),
            first_displayed_name_unelided, second_displayed_name_unelided);

    int unelided_width = gfx::GetStringWidth(label_text_unelided, font_list);

    std::u16string first_displayed_name;
    std::u16string second_displayed_name;

    if (unelided_width <= available_text_width) {
      // If the entire unelided text fits, use it as-is.
      first_displayed_name = first_displayed_name_unelided;
      second_displayed_name = second_displayed_name_unelided;
    } else {
      // Otherwise, fall back to equal-budget middle elision for each item.
      std::u16string boilerplate_text =
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(
                  IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_FILES_TEXT),
              base::checked_cast<int64_t>(model->RowCount()), std::u16string(),
              std::u16string());
      int boilerplate_width = gfx::GetStringWidth(boilerplate_text, font_list);

      int max_items = std::min(static_cast<size_t>(2), model->RowCount());
      int item_width =
          std::max(0, (available_text_width - boilerplate_width) / max_items);

      first_displayed_name = file_system_access_ui_helper::ElidePath(
          first_displayed_path.BaseName(), font_list, item_width);

      if (model->RowCount() > 1) {
        second_displayed_name = file_system_access_ui_helper::ElidePath(
            second_displayed_path.BaseName(), font_list, item_width);
      }
    }

    std::u16string label_text =
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_FILES_TEXT),
            base::checked_cast<int64_t>(model->RowCount()),
            first_displayed_name, second_displayed_name);
    label->SetText(label_text);

    // Construct an unelided tooltip for the collapsed label.
    std::u16string first_item_full_path = model->GetTooltip(0);
    if (model->RowCount() == 1) {
      tooltip_text = first_item_full_path;
    } else {
      tooltip_text = base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_FILES_TEXT),
          base::checked_cast<int64_t>(model->RowCount()), first_item_full_path,
          model->GetTooltip(1));
    }
    label->SetTooltipText(tooltip_text);

    summary_row_view->AddChildView(std::move(icon));
    auto* label_ptr = summary_row_view->AddChildView(std::move(label));
    row_layout->SetFlexForView(label_ptr, 1);

    expand_collapse_button_ = summary_row_view->AddChildView(std::move(button));

    int preferred_width = summary_row_view->GetPreferredSize().width();
    AddChildView(std::move(summary_row_view));

    ui::TableColumn column;
    column.elide_behavior = gfx::ELIDE_MIDDLE;
    column.percent = 1.0f;
    std::vector<ui::TableColumn> table_columns{column};
    auto table_view = std::make_unique<views::TableView>(
        model, std::move(table_columns), views::TableType::kIconAndText,
        /*single_selection=*/true);
    table_view->SetEnabled(false);
    int row_height = table_view->GetRowHeight();
    int table_height = table_view->GetPreferredSize().height();
    table_view_parent_ = AddChildView(
        views::TableView::CreateScrollViewWithTable(std::move(table_view)));
    // Ideally we'd use table_view_parent_->GetInsets().height(), but that only
    // returns the correct value after the view has been added to a root widget.
    // So just hardcode the inset height to 2 pixels as that is what the scroll
    // view uses.
    int inset_height = 2;
    table_view_parent_->SetPreferredSize(
        gfx::Size(preferred_width,
                  std::min(table_height, kExpandedTableRowCount * row_height) +
                      inset_height));
    table_view_parent_->SetVisible(false);
  }

  void ClearModel() {
    static_cast<views::TableView*>(table_view_parent_->contents())
        ->SetModel(nullptr);
  }

 private:
  void ButtonPressed() {
    table_is_expanded_ = !table_is_expanded_;
    expand_collapse_button_->SetToggled(table_is_expanded_);
    table_view_parent_->SetVisible(table_is_expanded_);
    PreferredSizeChanged();
  }

  bool table_is_expanded_ = false;
  raw_ptr<views::ScrollView> table_view_parent_;
  raw_ptr<views::ToggleImageButton> expand_collapse_button_;
};

BEGIN_METADATA(CollapsibleListView)
END_METADATA

}  // namespace

FileSystemAccessUsageBubbleView::Usage::Usage() = default;
FileSystemAccessUsageBubbleView::Usage::~Usage() = default;
FileSystemAccessUsageBubbleView::Usage::Usage(Usage&&) = default;
FileSystemAccessUsageBubbleView::Usage&
FileSystemAccessUsageBubbleView::Usage::operator=(Usage&&) = default;

FileSystemAccessUsageBubbleView::FilePathListModel::FilePathListModel(
    std::vector<base::FilePath> files,
    std::vector<base::FilePath> directories)
    : files_(std::move(files)), directories_(std::move(directories)) {}

FileSystemAccessUsageBubbleView::FilePathListModel::~FilePathListModel() =
    default;

size_t FileSystemAccessUsageBubbleView::FilePathListModel::RowCount() {
  return files_.size() + directories_.size();
}

std::u16string FileSystemAccessUsageBubbleView::FilePathListModel::GetText(
    size_t row,
    int column_id) {
  // Use the non-eliding version of GetPathForDisplay since these are files the
  // user has already granted the site access to.
  if (row < files_.size()) {
    return file_system_access_ui_helper::GetPathForDisplayAsParagraph(
        content::PathInfo(files_[row]));
  }
  return file_system_access_ui_helper::GetPathForDisplayAsParagraph(
      content::PathInfo(directories_[row - files_.size()]));
}

ui::ImageModel FileSystemAccessUsageBubbleView::FilePathListModel::GetIcon(
    size_t row) {
  return ui::ImageModel::FromVectorIcon(
      row < files_.size()                 ? features::IsRoundedIconsEnabled()
                                                ? vector_icons::kDraftIcon
                                                : vector_icons::kInsertDriveFileOutlineOldIcon
      : features::IsRoundedIconsEnabled() ? vector_icons::kFolderOpenIcon
                                          : vector_icons::kFolderOpenOldIcon,
      ui::kColorIcon, kIconSize);
}

std::u16string FileSystemAccessUsageBubbleView::FilePathListModel::GetTooltip(
    size_t row) {
  if (row < files_.size()) {
    return files_[row].LossyDisplayName();
  }
  return directories_[row - files_.size()].LossyDisplayName();
}

void FileSystemAccessUsageBubbleView::FilePathListModel::SetObserver(
    ui::TableModelObserver*) {}

// static
FileSystemAccessUsageBubbleView* FileSystemAccessUsageBubbleView::bubble_ =
    nullptr;

void FileSystemAccessUsageBubbleView::UpdateBubbleVisibilityState(
    bool is_bubble_visible) {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          bubble_->web_contents());
  if (!browser) {
    return;
  }
  auto* action_item = actions::ActionManager::Get().FindAction(
      kActionShowFileSystemAccess, browser->GetActions()->root_action_item());
  CHECK(action_item);
  action_item->SetIsShowingBubble(is_bubble_visible);
}

// static
void FileSystemAccessUsageBubbleView::ShowBubble(
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage) {
  base::RecordAction(
      base::UserMetricsAction("NativeFileSystemAPI.OpenedBubble"));

  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }

  ToolbarButtonProvider* button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();

  // Writable files or directories are generally also readable, but we don't
  // want to display the same path twice. So filter out any writable paths from
  // the readable lists.
  std::set<base::FilePath> writable_directories(
      usage.writable_directories.begin(), usage.writable_directories.end());
  std::erase_if(usage.readable_directories, [&](const base::FilePath& path) {
    return writable_directories.contains(path);
  });
  std::set<base::FilePath> writable_files(usage.writable_files.begin(),
                                          usage.writable_files.end());
  std::erase_if(usage.readable_files, [&](const base::FilePath& path) {
    return writable_files.contains(path);
  });

  // TODO(crbug.com/376282751): An action ID should be created and used here
  // when File System Access is migrated to the new page actions framework.
  bubble_ = new FileSystemAccessUsageBubbleView(
      button_provider->GetBubbleAnchor(std::nullopt), web_contents, origin,
      std::move(usage));

  bubble_->SetHighlightedElement(kFileSystemPageActionElementId);
  views::BubbleDialogDelegateView::CreateBubble(bubble_);

  bubble_->ShowForReason(DisplayReason::USER_GESTURE,
                         /*allow_refocus_alert=*/true);
  bubble_->UpdateBubbleVisibilityState(/*is_bubble_visible=*/true);
}

// static
void FileSystemAccessUsageBubbleView::CloseCurrentBubble() {
  if (bubble_) {
    bubble_->UpdateBubbleVisibilityState(/*is_bubble_visible=*/false);
    bubble_->CloseBubble();
  }
}

// static
FileSystemAccessUsageBubbleView* FileSystemAccessUsageBubbleView::GetBubble() {
  return bubble_;
}

FileSystemAccessUsageBubbleView::FileSystemAccessUsageBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage)
    : LocationBarBubbleDelegateView(anchor,
                                    web_contents,
                                    /*autosize=*/true),
      origin_(origin),
      usage_(std::move(usage)),
      readable_paths_model_(std::move(usage_.readable_files),
                            std::move(usage_.readable_directories)),
      writable_paths_model_(std::move(usage_.writable_files),
                            std::move(usage_.writable_directories)) {
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_DONE));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_ACCESS_USAGE_REMOVE_ACCESS));
  SetCancelCallback(
      base::BindOnce(&FileSystemAccessUsageBubbleView::OnDialogCancelled,
                     base::Unretained(this)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

FileSystemAccessUsageBubbleView::~FileSystemAccessUsageBubbleView() {
  if (readable_collapsible_list_view_) {
    static_cast<CollapsibleListView*>(readable_collapsible_list_view_)
        ->ClearModel();
  }
  if (writable_collapsible_list_view_) {
    static_cast<CollapsibleListView*>(writable_collapsible_list_view_)
        ->ClearModel();
  }
}

std::u16string FileSystemAccessUsageBubbleView::GetAccessibleWindowTitle()
    const {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          web_contents());
  // Don't crash if the web_contents is destroyed/unloaded.
  if (!browser) {
    return {};
  }

  auto* page_action_view =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetPageActionViewInterface(kActionShowFileSystemAccess);
  if (!page_action_view) {
    return {};
  }
  return page_action_view->GetTooltipText();
}

bool FileSystemAccessUsageBubbleView::ShouldShowCloseButton() const {
  return true;
}

void FileSystemAccessUsageBubbleView::Init() {
  // Set up the layout of the bubble.
  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(0, dialog_insets.left(), 0, dialog_insets.right()),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(gfx::Insets::TLBR(
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT),
      0,
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
      0));

  base::FilePath embedded_path;
  int heading_message_id =
      ComputeHeadingMessageFromUsage(usage_, &embedded_path);

  if (!embedded_path.empty()) {
    AddChildView(file_system_access_ui_helper::CreateOriginPathLabel(
        web_contents(), heading_message_id, origin_, embedded_path,
        views::style::CONTEXT_DIALOG_BODY_TEXT,
        /*show_emphasis=*/false));
  } else {
    AddChildView(file_system_access_ui_helper::CreateOriginLabel(
        web_contents(), heading_message_id, origin_,
        views::style::CONTEXT_DIALOG_BODY_TEXT,
        /*show_emphasis=*/false));

    if (writable_paths_model_.RowCount() > 0) {
      if (readable_paths_model_.RowCount() > 0) {
        auto label = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SAVE_CHANGES),
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            views::style::STYLE_PRIMARY);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(std::move(label));
      }
      writable_collapsible_list_view_ = AddChildView(
          std::make_unique<CollapsibleListView>(&writable_paths_model_));
    }

    if (readable_paths_model_.RowCount() > 0) {
      if (writable_paths_model_.RowCount() > 0) {
        auto label = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_CHANGES),
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            views::style::STYLE_PRIMARY);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(std::move(label));
      }
      readable_collapsible_list_view_ = AddChildView(
          std::make_unique<CollapsibleListView>(&readable_paths_model_));
    }
  }
}

void FileSystemAccessUsageBubbleView::OnDialogCancelled() {
  base::RecordAction(
      base::UserMetricsAction("NativeFileSystemAPI.RevokePermissions"));

  if (!web_contents()) {
    return;
  }

  content::BrowserContext* profile = web_contents()->GetBrowserContext();
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(profile);
  if (!context) {
    return;
  }

  context->RevokeGrants(origin_);
}

void FileSystemAccessUsageBubbleView::WindowClosing() {
  // |bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to nullptr when it's this bubble.
  if (bubble_ == this) {
    UpdateBubbleVisibilityState(/*is_bubble_visible=*/false);
    bubble_ = nullptr;
  }
}

void FileSystemAccessUsageBubbleView::CloseBubble() {
  // Widget's Close() is async, but we don't want to use bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  UpdateBubbleVisibilityState(/*is_bubble_visible=*/false);
  bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

BEGIN_METADATA(FileSystemAccessUsageBubbleView)
END_METADATA
