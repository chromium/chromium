// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/ambient_login_permission_bubble_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/permissions/ambient_login_credential_icon.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// The size of vector icons.
constexpr int kIconSize = ambient_login_credential_icon::kVectorIconSize;
constexpr int kBadgeImageSize = ambient_signin::kBadgeImageSize;
constexpr int kViewportBottomMargin = 48;

// The padding on the left and right of the title and the credentials.
constexpr int kHorizontalPadding = 20;
// The padding above and below each credential.
constexpr int kCredentialVerticalPadding = 10;
// The padding below the credentials, in addition to their own padding.
constexpr int kBottomPadding = 4;

// The IdP name of a federated credential is a domain, e.g. "idp.example".
// Domains are elided at the start, since their end is the most significant
// part.
constexpr gfx::ElideBehavior kIdpNameElideBehavior = gfx::ELIDE_HEAD;

// A badge is a circle near the bottom right corner of a picture, with the
// badge's image (`kBadgeImageSize` DIPs) in the middle, e.g. the identity
// provider's icon on an account picture, or the password manager's icon on the
// monogram of a passkey or password. The circle is transparent around the
// image, and it casts a shadow.
constexpr int kBadgeRadius = 10;
static_assert(2 * kBadgeRadius >= kBadgeImageSize,
              "The badge's image must fit in its circle");
// How far the badge's circle extends beyond the right and bottom edges of the
// picture.
constexpr int kBadgeOverhang = 2;

// A shadow of the badge's circle, in DIPs.
struct BadgeShadow {
  int y_offset;
  int spread;
  // Like the blur radius of a CSS box-shadow, i.e. twice the standard deviation
  // of the blur.
  int blur;
  SkAlpha alpha;
};

// The shadows of Material elevation level 1: a key shadow, and a lighter but
// larger ambient shadow.
constexpr BadgeShadow kBadgeShadows[] = {
    {.y_offset = 1, .spread = 0, .blur = 2, .alpha = 0x4D},
    {.y_offset = 1, .spread = 1, .blur = 3, .alpha = 0x26},
};

// How far the shadows extend beyond the badge's circle.
constexpr int kBadgeShadowMargin = 5;
static_assert(std::ranges::all_of(kBadgeShadows, [](const BadgeShadow& shadow) {
  return shadow.y_offset + shadow.spread + shadow.blur <= kBadgeShadowMargin;
}));

// The size of a picture with a badge, including the parts of the badge and its
// shadows that extend beyond the picture.
constexpr int kBadgedPictureSize =
    ambient_signin::kAccountPictureSize + kBadgeOverhang + kBadgeShadowMargin;

// Returns the spacing between the icon and the text of a credential, in both
// the collapsed and the expanded bubble.
int GetIconTextSpacing() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
}

// Returns the icon shown when there is no more specific icon, e.g. for a
// federated credential without an account picture.
ui::ImageModel GetDefaultCredentialIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon, kIconSize);
}

// Returns the icon for a federated credential: the account picture if
// available, otherwise a default icon.
ui::ImageModel GetFederatedCredentialIcon(
    const ambient_signin::FederatedCredential& credential) {
  if (!credential.icon.IsEmpty()) {
    // The picture is square (see FederatedCredential::icon), so it isn't
    // stretched when shown at `ambient_signin::kAccountPictureSize`.
    CHECK_EQ(credential.icon.Size().width(), credential.icon.Size().height(),
             base::NotFatalUntil::M162);
    return credential.icon;
  }
  return GetDefaultCredentialIcon();
}

// Draws a picture with a badge near its bottom right corner, as described
// above `kBadgeRadius`.
class BadgedPictureSource : public gfx::CanvasImageSource {
 public:
  BadgedPictureSource(gfx::ImageSkia picture, gfx::ImageSkia badge)
      : gfx::CanvasImageSource(
            gfx::Size(kBadgedPictureSize, kBadgedPictureSize)),
        picture_(std::move(picture)),
        badge_(std::move(badge)) {}

  BadgedPictureSource(const BadgedPictureSource&) = delete;
  BadgedPictureSource& operator=(const BadgedPictureSource&) = delete;

  ~BadgedPictureSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    constexpr int kPictureSize = ambient_signin::kAccountPictureSize;
    constexpr int kBadgeCenter = kPictureSize + kBadgeOverhang - kBadgeRadius;
    {
      // Neither the picture nor the shadows cover the badge's circle, so that
      // what is behind the picture shows around the badge's image.
      gfx::ScopedCanvas scoped_canvas(canvas);
      canvas->ClipPath(SkPath::Circle(kBadgeCenter, kBadgeCenter, kBadgeRadius),
                       /*do_anti_alias=*/true, SkClipOp::kDifference);
      canvas->DrawImageInt(picture_, 0, 0, picture_.width(), picture_.height(),
                           0, 0, kPictureSize, kPictureSize, /*filter=*/true);
      for (const BadgeShadow& shadow : kBadgeShadows) {
        cc::PaintFlags flags;
        flags.setAntiAlias(true);
        flags.setColor(SkColorSetA(SK_ColorBLACK, shadow.alpha));
        const float sigma = shadow.blur / 2.0f;
        flags.setImageFilter(sk_make_sp<cc::BlurPaintFilter>(
            sigma, sigma, SkTileMode::kDecal, /*input=*/nullptr));
        canvas->DrawCircle(
            gfx::PointF(kBadgeCenter, kBadgeCenter + shadow.y_offset),
            kBadgeRadius + shadow.spread, flags);
      }
    }
    constexpr int kBadgeImageOrigin = kBadgeCenter - kBadgeImageSize / 2;
    canvas->DrawImageInt(badge_, 0, 0, badge_.width(), badge_.height(),
                         kBadgeImageOrigin, kBadgeImageOrigin, kBadgeImageSize,
                         kBadgeImageSize, /*filter=*/true);
  }

 private:
  const gfx::ImageSkia picture_;
  const gfx::ImageSkia badge_;
};

// Returns `picture` with `badge` near its bottom right corner. Both are
// rasterized when the result is painted, so that they follow the colors of the
// bubble, e.g. if `badge` is a vector icon.
ui::ImageModel CreateBadgedPicture(const ui::ImageModel& picture,
                                   const ui::ImageModel& badge) {
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(
          [](const ui::ImageModel& picture, const ui::ImageModel& badge,
             const ui::ColorProvider* color_provider) {
            return gfx::CanvasImageSource::MakeImageSkia<BadgedPictureSource>(
                picture.Rasterize(color_provider),
                badge.Rasterize(color_provider));
          },
          picture, badge),
      gfx::Size(kBadgedPictureSize, kBadgedPictureSize));
}

// Shows a picture with a badge near its bottom right corner. Only the picture
// takes up space in the layout, so that the badge extends into the padding
// around it rather than moving the text next to it.
class BadgedPictureView : public views::View {
  METADATA_HEADER(BadgedPictureView, views::View)

 public:
  BadgedPictureView(const ui::ImageModel& picture,
                    const ui::ImageModel& badge) {
    SetPreferredSize(gfx::Size(ambient_signin::kAccountPictureSize,
                               ambient_signin::kAccountPictureSize));
    // The badged picture is wider than this view, so mirroring its position in
    // right-to-left UIs would move the picture out of place. The badge stays
    // on the bottom right corner instead.
    SetMirrored(false);
    // Screen readers skip the picture, like a picture without a badge.
    GetViewAccessibility().SetIsIgnored(true);
    auto image_view =
        std::make_unique<views::ImageView>(CreateBadgedPicture(picture, badge));
    // A layer is necessary to paint beyond the bounds of this view.
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);
    image_view->SizeToPreferredSize();
    AddChildView(std::move(image_view));
  }
};

BEGIN_METADATA(BadgedPictureView)
END_METADATA

// Returns a view that shows `icon`. Vector icons already have their size,
// whereas pictures, i.e. other images such as account pictures and monograms,
// are shown at `ambient_signin::kAccountPictureSize`, with `badge` near their
// bottom right corner if it isn't empty.
std::unique_ptr<views::View> CreateIconView(
    const ui::ImageModel& icon,
    const ui::ImageModel& badge = ui::ImageModel()) {
  if (icon.IsVectorIcon()) {
    return std::make_unique<views::ImageView>(icon);
  }
  if (!badge.IsEmpty()) {
    // The badge is square (see `kBadgeImageSize`), so it isn't stretched.
    CHECK_EQ(badge.Size().width(), badge.Size().height(),
             base::NotFatalUntil::M162);
    return std::make_unique<BadgedPictureView>(icon, badge);
  }
  auto picture_view = std::make_unique<views::ImageView>(icon);
  picture_view->SetImageSize(gfx::Size(ambient_signin::kAccountPictureSize,
                                       ambient_signin::kAccountPictureSize));
  return picture_view;
}

// A row for a credential in the expanded bubble. It has the same padding and
// text styles as the credential in the collapsed bubble.
class CredentialRow : public HoverButton {
  METADATA_HEADER(CredentialRow, HoverButton)

 public:
  CredentialRow(PressedCallback callback, Params params)
      : HoverButton(std::move(callback),
                    WithIconTextSpacing(std::move(params))) {
    // HoverButton adds half of DISTANCE_CONTROL_LIST_VERTICAL above and below
    // the icon and the labels, so the border only adds the rest.
    const int vertical_border = kCredentialVerticalPadding -
                                views::LayoutProvider::Get()->GetDistanceMetric(
                                    views::DISTANCE_CONTROL_LIST_VERTICAL) /
                                    2;
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(vertical_border, kHorizontalPadding)));
    title()->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
    SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                         views::style::STYLE_BODY_4);
    SetFooterTextStyle(views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4);
    for (views::Label* label : {subtitle(), footer()}) {
      if (label) {
        label->SetEnabledColor(ui::kColorLabelForegroundSecondary);
      }
    }
  }

 private:
  // Returns `params` with the same spacing between the icon and the text as
  // in the collapsed bubble, so that the text stays aligned when the bubble is
  // expanded.
  static Params WithIconTextSpacing(Params params) {
    params.icon_label_spacing = GetIconTextSpacing();
    return params;
  }
};

BEGIN_METADATA(CredentialRow)
END_METADATA

// A row for a federated credential in the expanded bubble.
class FederatedCredentialRow : public CredentialRow {
  METADATA_HEADER(FederatedCredentialRow, CredentialRow)

 public:
  // `idp_name_in_footer` is whether the IdP name is `params.footer` rather
  // than `params.subtitle`.
  FederatedCredentialRow(PressedCallback callback,
                         Params params,
                         bool idp_name_in_footer)
      : CredentialRow(std::move(callback), std::move(params)) {
    // There is no label for the IdP name if it is empty.
    if (views::Label* idp_label = idp_name_in_footer ? footer() : subtitle()) {
      idp_label->SetElideBehavior(kIdpNameElideBehavior);
    }
  }
};

BEGIN_METADATA(FederatedCredentialRow)
END_METADATA

}  // namespace

AmbientLoginPermissionBubbleView::AmbientLoginPermissionBubbleView(
    content::WebContents* web_contents,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    PermissionPromptStyle prompt_style)
    : PermissionPromptBubbleBaseView(web_contents, delegate, prompt_style),
      site_color_(ambient_login_credential_icon::GetSiteColorForWebContents(
          web_contents)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_title_margins(
      gfx::Insets::TLBR(12, kHorizontalPadding, 8, kHorizontalPadding));

  SetTitle(l10n_util::GetStringUTF16(IDS_AMBIENT_LOGIN_TITLE));

  std::u16string username;
  std::u16string provider;
  gfx::ElideBehavior provider_elide_behavior = gfx::ELIDE_TAIL;
  ui::ImageModel credential_icon = GetDefaultCredentialIcon();
  ui::ImageModel credential_badge;
  CHECK_EQ(delegate->Requests().size(), 1u);
  ambient_signin::AmbientLoginPermissionRequest* ambient_request =
      GetAmbientRequest();
  CHECK(ambient_request);
  if (!ambient_request->credentials().empty()) {
    const ambient_signin::PasskeyOrPasswordCredential& cred =
        ambient_request->credentials().front();
    username = cred.username;
    provider = cred.provider_name;
    credential_icon =
        ambient_login_credential_icon::CreateCredentialIcon(cred, site_color_);
    credential_badge = ambient_login_credential_icon::GetCredentialBadge(cred);
  } else if (!ambient_request->federated_credentials().empty()) {
    const ambient_signin::FederatedCredential& fed_cred =
        ambient_request->federated_credentials().front();
    username =
        !fed_cred.account_name.empty() ? fed_cred.account_name : fed_cred.email;
    provider = fed_cred.idp_name;
    provider_elide_behavior = kIdpNameElideBehavior;
    credential_icon = GetFederatedCredentialIcon(fed_cred);
  }

  if (username.empty()) {
    username = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);

  AddChildView(CreateIconView(credential_icon, credential_badge));

  auto text_container = std::make_unique<views::View>();
  text_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, GetIconTextSpacing(), 0,
                        views::LayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  auto* text_layout =
      text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  text_layout->set_cross_axis_alignment(views::LayoutAlignment::kStart);

  auto username_label = std::make_unique<views::Label>(
      username, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_3_MEDIUM);
  username_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  username_label->SetElideBehavior(gfx::ELIDE_TAIL);
  text_container->AddChildView(std::move(username_label));

  if (!provider.empty()) {
    auto provider_label = std::make_unique<views::Label>(
        provider, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4);
    provider_label->SetEnabledColor(ui::kColorLabelForegroundSecondary);
    provider_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    provider_label->SetElideBehavior(provider_elide_behavior);
    text_container->AddChildView(std::move(provider_label));
  }

  // The bubble has a fixed width (see PermissionPromptBubbleBaseView), so the
  // text shrinks to leave room for the buttons and long text is elided.
  layout->SetFlexForView(AddChildView(std::move(text_container)), 1);

  auto sign_in_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &AmbientLoginPermissionBubbleView::OnSignInButtonClicked,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
  sign_in_button->SetStyle(ui::ButtonStyle::kProminent);
  sign_in_button->SetProperty(views::kElementIdentifierKey,
                              kAllowButtonElementId);
  sign_in_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0,
                        views::LayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  std::u16string button_description =
      provider.empty() ? username : base::StrCat({username, u" ", provider});
  sign_in_button->GetViewAccessibility().SetDescription(button_description);
  sign_in_button_ = AddChildView(std::move(sign_in_button));

  std::unique_ptr<views::ImageButton> expand_button =
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              &AmbientLoginPermissionBubbleView::OnExpandButtonClicked,
              base::Unretained(this)),
          kUnfoldMoreIcon, kIconSize);
  expand_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ANCHORED_MESSAGE_EXPAND_BUTTON_TOOLTIP));
  views::InstallCircleHighlightPathGenerator(expand_button.get());
  // The expand button's icon, rather than the button, is aligned with the
  // padding on the right.
  set_margins(gfx::Insets::TLBR(
      kCredentialVerticalPadding, kHorizontalPadding,
      kCredentialVerticalPadding + kBottomPadding,
      kHorizontalPadding - expand_button->GetInsets().right()));
  AddChildView(std::move(expand_button));
}

AmbientLoginPermissionBubbleView::~AmbientLoginPermissionBubbleView() = default;

void AmbientLoginPermissionBubbleView::Show() {
  CHECK(GetNativeWindow());
  UpdateAnchorPosition();
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
  ShowWidget();
}

void AmbientLoginPermissionBubbleView::AddedToWidget() {
  PermissionPromptBubbleBaseView::AddedToWidget();

  auto title_label = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_BODY_3_MEDIUM);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  title_label->GetViewAccessibility().SetHierarchicalLevel(1);
  GetBubbleFrameView()->SetTitleView(std::move(title_label));
}

ambient_signin::AmbientLoginPermissionRequest*
AmbientLoginPermissionBubbleView::GetAmbientRequest() {
  if (!delegate() || delegate()->Requests().empty() ||
      delegate()->Requests().front()->request_type() !=
          permissions::RequestType::kAmbientLogin) {
    return nullptr;
  }
  return static_cast<ambient_signin::AmbientLoginPermissionRequest*>(
      delegate()->Requests().front().get());
}

void AmbientLoginPermissionBubbleView::OnSignInButtonClicked(
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false)) {
    return;
  }
  if (ShouldIgnoreButtonPressedEventHandling(sign_in_button_, event)) {
    return;
  }
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    if (!ambient_request->credentials().empty()) {
      ambient_request->SelectCredential(0);
    } else if (!ambient_request->federated_credentials().empty()) {
      ambient_request->SelectFederatedCredential(0);
    }
  }
  RunButtonCallback(static_cast<int>(PermissionDialogButton::kAccept));
}

void AmbientLoginPermissionBubbleView::OnExpandButtonClicked(
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false) ||
      ShouldIgnoreButtonPressedEventHandling(nullptr, event)) {
    return;
  }

  // Transitioning from the collapsed single-credential summary to the expanded
  // credential list replaces all child views, including `sign_in_button_` and
  // the expand button that triggered this callback. Clearing `sign_in_button_`
  // first avoids a dangling `raw_ptr`. Destruction of a clicked button during
  // its listener callback is explicitly supported (eg., see comment in
  // ui::views::ButtonController::OnMousePressed()).
  sign_in_button_ = nullptr;
  RemoveAllChildViews();
  set_margins(gfx::Insets::TLBR(0, 0, kBottomPadding, 0));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto list_view = std::make_unique<views::View>();
  views::BoxLayout* list_layout =
      list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  list_layout->set_cross_axis_alignment(views::LayoutAlignment::kStretch);

  views::View* first_row = nullptr;
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    for (size_t i = 0; i < ambient_request->credentials().size(); ++i) {
      HoverButton* row = list_view->AddChildView(
          CreateCredentialRow(ambient_request->credentials()[i], i));
      if (!first_row) {
        first_row = row;
      }
    }
    for (size_t i = 0; i < ambient_request->federated_credentials().size();
         ++i) {
      HoverButton* row = list_view->AddChildView(CreateFederatedCredentialRow(
          ambient_request->federated_credentials()[i], i));
      if (!first_row) {
        first_row = row;
      }
    }
  }

  views::ScrollView* scroll_view =
      AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(list_view));

  int max_scroll_height = scroll_view->GetPreferredSize().height();
  if (web_contents()) {
    const gfx::Rect viewport_bounds = web_contents()->GetContainerBounds();
    if (!viewport_bounds.IsEmpty()) {
      const gfx::Rect unclipped_bubble_bounds =
          GetBubbleFrameView()->GetUpdatedWindowBounds(
              GetAnchorRect(), arrow(),
              GetWidget()->client_view()->GetPreferredSize({}),
              /*adjust_to_fit_available_bounds=*/false);
      const int max_bubble_bottom =
          viewport_bounds.bottom() - kViewportBottomMargin;
      if (unclipped_bubble_bounds.bottom() > max_bubble_bottom) {
        const int overflow =
            unclipped_bubble_bounds.bottom() - max_bubble_bottom;
        max_scroll_height = std::max(0, max_scroll_height - overflow);
      }
    }
  }
  scroll_view->ClipHeightTo(0, max_scroll_height);

  SizeToContents();
  if (first_row) {
    first_row->RequestFocus();
  }
}

void AmbientLoginPermissionBubbleView::OnCredentialClicked(
    size_t index,
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false) ||
      ShouldIgnoreButtonPressedEventHandling(nullptr, event)) {
    return;
  }
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    ambient_request->SelectCredential(index);
  }
  RunButtonCallback(static_cast<int>(PermissionDialogButton::kAccept));
}

void AmbientLoginPermissionBubbleView::OnFederatedCredentialClicked(
    size_t index,
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false) ||
      ShouldIgnoreButtonPressedEventHandling(nullptr, event)) {
    return;
  }
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    ambient_request->SelectFederatedCredential(index);
  }
  RunButtonCallback(static_cast<int>(PermissionDialogButton::kAccept));
}

std::unique_ptr<HoverButton>
AmbientLoginPermissionBubbleView::CreateCredentialRow(
    const ambient_signin::PasskeyOrPasswordCredential& credential,
    size_t index) {
  std::u16string username =
      credential.username.empty()
          ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
          : credential.username;

  HoverButton::Params params;
  params.icon_view = CreateIconView(
      ambient_login_credential_icon::CreateCredentialIcon(credential,
                                                          site_color_),
      ambient_login_credential_icon::GetCredentialBadge(credential));
  if (credential.type == ambient_signin::CredentialType::kPasskey &&
      !credential.display_name.empty() && credential.display_name != username) {
    params.title = credential.display_name;
    params.subtitle = username;
    params.footer = credential.provider_name;
  } else {
    params.title = username;
    params.subtitle = credential.provider_name;
  }

  return std::make_unique<CredentialRow>(
      base::BindRepeating(
          &AmbientLoginPermissionBubbleView::OnCredentialClicked,
          base::Unretained(this), index),
      std::move(params));
}

std::unique_ptr<HoverButton>
AmbientLoginPermissionBubbleView::CreateFederatedCredentialRow(
    const ambient_signin::FederatedCredential& credential,
    size_t index) {
  HoverButton::Params params;
  params.icon_view = CreateIconView(GetFederatedCredentialIcon(credential));
  const bool has_name_and_email =
      !credential.account_name.empty() && !credential.email.empty();
  if (has_name_and_email) {
    params.title = credential.account_name;
    params.subtitle = credential.email;
    params.footer = credential.idp_name;
  } else {
    std::u16string identifier = !credential.account_name.empty()
                                    ? credential.account_name
                                    : credential.email;
    if (identifier.empty()) {
      identifier = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
    }
    params.title = identifier;
    params.subtitle = credential.idp_name;
  }

  return std::make_unique<FederatedCredentialRow>(
      base::BindRepeating(
          &AmbientLoginPermissionBubbleView::OnFederatedCredentialClicked,
          base::Unretained(this), index),
      std::move(params), /*idp_name_in_footer=*/has_name_and_email);
}

BEGIN_METADATA(AmbientLoginPermissionBubbleView)
END_METADATA
