// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace dictation {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationBubbleUi,
                                      kViewElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationBubbleUi,
                                      kCloseButtonElementIdForTesting);

namespace {

// The contents view of the dictation toast.
class DictationToastView : public views::View {
  METADATA_HEADER(DictationToastView, views::View)
 public:
  explicit DictationToastView(base::RepeatingClosure close_callback);
  ~DictationToastView() override;

  void Init();

 private:
  base::RepeatingClosure close_callback_;
};

}  // namespace

// --- DictationToastView ---

DictationToastView::DictationToastView(base::RepeatingClosure close_callback)
    : close_callback_(std::move(close_callback)) {
  SetProperty(views::kElementIdentifierKey,
              DictationBubbleUi::kViewElementIdForTesting);
}

DictationToastView::~DictationToastView() = default;

void DictationToastView::Init() {
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  // TODO(b/510778034): Determine what we need to make this accessibility
  // friendly.
  // TODO(b/510738735): Finalize placeholder strings.
  // TODO(b/512495405): Wrap the visual aspects of the view into a model so this
  // setup is common across elements..
  views::Label* label_view = AddChildView(
      std::make_unique<views::Label>(u"<placehold>", CONTEXT_TOAST_BODY_TEXT));
  label_view->SetEnabledColor(ui::kColorToastForeground);
  label_view->SetMultiLine(false);
  label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_view->SetAllowCharacterBreak(false);
  label_view->SetAutoColorReadabilityEnabled(false);
  label_view->SetLineHeight(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT));
  label_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING),
          0, 0));
  label_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero));

  views::MdTextButton* done_button =
      AddChildView(std::make_unique<views::MdTextButton>(
          base::RepeatingClosure(), l10n_util::GetStringUTF16(IDS_DONE)));
  done_button->SetEnabledTextColors(ui::kColorToastButton);
  done_button->SetBgColorIdOverride(ui::kColorToastBackgroundProminent);
  done_button->SetStrokeColorIdOverride(ui::kColorToastButton);
  done_button->SetPreferredSize(gfx::Size(
      done_button->GetPreferredSize().width(),
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)));
  done_button->SetStyle(ui::ButtonStyle::kProminent);
  done_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0,
          lp->GetDistanceMetric(
              DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING),
          0, 0));

  views::ImageButton* close_button =
      AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          close_callback_,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kCloseIcon
              : vector_icons::kCloseChromeRefreshOldIcon,
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE),
          ui::kColorToastForeground, ui::kColorIconDisabled,
          ui::kColorToastForeground));
  const gfx::Insets insets =
      lp->GetInsetsMetric(views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON);
  close_button->SetBorder(views::CreateEmptyBorder(insets));
  views::InstallCircleHighlightPathGenerator(close_button);
  close_button->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING),
          0, 0));
  close_button->SetProperty(views::kElementIdentifierKey,
                            DictationBubbleUi::kCloseButtonElementIdForTesting);
}

BEGIN_METADATA(DictationToastView)
END_METADATA

// --- DictationToastBubbleDelegate ---

DictationBubbleUi::DictationBubbleUi(views::View* anchor_view,
                                     base::RepeatingClosure close_callback)
    : BubbleDialogDelegate(anchor_view, views::BubbleBorder::NONE) {
  SetBackgroundColor(ui::kColorToastBackgroundProminent);
  SetShowCloseButton(false);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  set_close_on_deactivate(false);
  SetContentsView(std::make_unique<DictationToastView>(close_callback));

  // TODO(crbug.com/509983464): Update this to call an undeprecated factory
  // function when this bug is fixed.
  widget_ =
      base::WrapUnique(views::BubbleDialogDelegate::CreateBubbleDeprecated(
          this, views::Widget::InitParams::CLIENT_OWNS_WIDGET));

  GetBubbleFrameView()->bubble_border()->set_draw_border_stroke(false);
}

DictationBubbleUi::~DictationBubbleUi() = default;

void DictationBubbleUi::Show() {
  CHECK(widget_);
  widget_->ShowInactive();
}

void DictationBubbleUi::Init() {
  CHECK(GetContentsView());
  views::AsViewClass<DictationToastView>(GetContentsView())->Init();

  const auto* const layout_provider = ChromeLayoutProvider::Get();
  const gfx::Insets insets = layout_provider->GetInsetsMetric(
      views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON);
  const int max_child_height = std::max(
      {layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT),
       layout_provider->GetDistanceMetric(
           DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON),
       layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE) +
           insets.height()});

  const int total_vertical_margins =
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) -
      max_child_height;
  const int top_margin = total_vertical_margins / 2;

  set_margins(gfx::Insets::TLBR(
      top_margin,
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins - top_margin,
      layout_provider->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON)));
}

gfx::Rect DictationBubbleUi::GetBubbleBounds() {
  views::View* anchor_view = GetAnchorView();
  if (!anchor_view) {
    return gfx::Rect();
  }

  const gfx::Size preferred_size =
      GetWidget()->GetContentsView()->GetPreferredSize();
  const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();

  const int minimum_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 DISTANCE_TOAST_BUBBLE_BROWSER_WINDOW_MARGIN) -
                             views::BubbleBorder::kShadowBlur;
  const int width =
      std::min(preferred_size.width(),
               std::max(anchor_bounds.width() - 2 * minimum_margin, 0));
  const int x = anchor_bounds.x() + ((anchor_bounds.width() - width) / 2);

  const int y = anchor_bounds.bottom() - (preferred_size.height() / 2);
  return gfx::Rect(x, y, width, preferred_size.height());
}

}  // namespace dictation
