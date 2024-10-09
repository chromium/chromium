// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <string>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

std::unique_ptr<views::Label> CreateFormattedLabel(
    const std::u16string& message) {
  auto label = std::make_unique<views::Label>(
      message, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetLineHeight(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  return label;
}

// Return a string describing the error code. Keep in sync with the
// CrashExitCodes in /tools/metrics/histograms/enums.xml.
std::u16string ErrorToString(int error_code) {
  std::string error_string;
  switch (std::abs(error_code)) {
    case 1:
      error_string = "RESULT_CODE_KILLED";
      break;
    case 2:
      error_string = "RESULT_CODE_HUNG";
      break;
    case 3:
      error_string = "RESULT_CODE_KILLED_BAD_MESSAGE";
      break;
    // Code 4 conflicts between SIGILL and RESULT_CODE_GPU_DEAD_ON_ARRIVAL.
    // Code 5 conflicts between SIGTRAP and RESULT_CODE_INVALID_CMDLINE_URL.
    // Code 6 conflicts between SIGABRT and RESULT_CODE_BAD_PROCESS_TYPE.
    // Omit these to show the default error string.
    case 7:
      error_string = "RESULT_CODE_MISSING_DATA";
      break;
    // Codes 8-12 conflict between various signals and various uninstaller error
    // codes. Omit them to show the default error strings.
    case 13:
      error_string = "RESULT_CODE_UNSUPPORTED_PARAM";
      break;
    case 14:
      error_string = "RESULT_CODE_IMPORTER_HUNG";
      break;
    // Code 15 conflicts between SIGTERM and RESULT_CODE_RESPAWN_FAILED. Omit
    // it to show the default error string.
    case 16:
      error_string = "RESULT_CODE_NORMAL_EXIT_EXP1";
      break;
    case 17:
      error_string = "RESULT_CODE_NORMAL_EXIT_EXP2";
      break;
    case 18:
      error_string = "RESULT_CODE_NORMAL_EXIT_EXP3";
      break;
    case 19:
      error_string = "RESULT_CODE_NORMAL_EXIT_EXP4";
      break;
    case 20:
      error_string = "RESULT_CODE_NORMAL_EXIT_CANCEL";
      break;
    case 21:
      error_string = "RESULT_CODE_PROFILE_IN_USE";
      break;
    case 22:
      error_string = "RESULT_CODE_PACK_EXTENSION_ERROR";
      break;
    case 23:
      error_string = "RESULT_CODE_UNINSTALL_EXTENSION_ERROR";
      break;
    case 24:
      error_string = "RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED";
      break;
    case 26:
      error_string = "RESULT_CODE_INSTALL_FROM_WEBSTORE_ERROR_2";
      break;
    case 28:
      error_string = "RESULT_CODE_EULA_REFUSED";
      break;
    case 29:
      error_string = "RESULT_CODE_SXS_MIGRATION_FAILED_NOT_USED";
      break;
    case 30:
      error_string = "RESULT_CODE_ACTION_DISALLOWED_BY_POLICY";
      break;
    case 31:
      error_string = "RESULT_CODE_INVALID_SANDBOX_STATE";
      break;
    case 32:
      error_string = "RESULT_CODE_CLOUD_POLICY_ENROLLMENT_FAILED";
      break;
    case 33:
      error_string = "RESULT_CODE_DOWNGRADE_AND_RELAUNCH";
      break;
    case 34:
      error_string = "RESULT_CODE_GPU_EXIT_ON_CONTEXT_LOST";
      break;
    case 131:
      error_string = "SIGQUIT";
      break;
    case 132:
      error_string = "SIGILL";
      break;
    case 133:
      error_string = "SIGTRAP";
      break;
    case 134:
      error_string = "SIGABRT";
      break;
    case 135:
      error_string = "SIGBUS (7)";
      break;
    case 136:
      error_string = "SIGFPE";
      break;
    case 137:
      error_string = "SIGKILL";
      break;
    case 138:
      error_string = "SIGBUS (10)";
      break;
    case 139:
      error_string = "SIGSEGV";
      break;
    case 140:
      error_string = "SIGSYS";
      break;
    case 258:
      error_string = "WAIT_TIMEOUT";
      break;
    case 7006:
      error_string = "SBOX_FATAL_INTEGRITY";
      break;
    case 7007:
      error_string = "SBOX_FATAL_DROPTOKEN";
      break;
    case 7008:
      error_string = "SBOX_FATAL_FLUSHANDLES";
      break;
    case 7009:
      error_string = "SBOX_FATAL_CACHEDISABLE";
      break;
    case 7010:
      error_string = "SBOX_FATAL_CLOSEHANDLES";
      break;
    case 7011:
      error_string = "SBOX_FATAL_MITIGATION";
      break;
    case 7012:
      error_string = "SBOX_FATAL_MEMORY_EXCEEDED";
      break;
    case 7013:
      error_string = "SBOX_FATAL_WARMUP";
      break;
    case 7014:
      error_string = "SBOX_FATAL_BROKER_SHUTDOWN_HUNG";
      break;
    case 36861:
      error_string = "Crashpad_NotConnectedToHandler";
      break;
    case 36862:
      error_string = "Crashpad_FailedToCaptureProcess";
      break;
    case 36863:
      error_string = "Crashpad_HandlerDidNotRespond";
      break;
    case 85436397:
      error_string = "Crashpad_SimulatedCrash";
      break;
    case 529697949:
      error_string = "CPP_EH_EXCEPTION";
      break;
    case 533692099:
      error_string = "STATUS_GUARD_PAGE_VIOLATION";
      break;
    case 536870904:
      error_string = "Out of Memory";
      break;
    case 1066598273:
      error_string = "FACILITY_VISUALCPP/ERROR_PROC_NOT_FOUND";
      break;
    case 1066598274:
      error_string = "FACILITY_VISUALCPP/ERROR_MOD_NOT_FOUND";
      break;
    case 1073740760:
      error_string = "STATUS_INVALID_IMAGE_HASH";
      break;
    case 1073740791:
      error_string = "STATUS_STACK_BUFFER_OVERRUN";
      break;
    case 1073740940:
      error_string = "STATUS_HEAP_CORRUPTION";
      break;
    case 1073741502:
      error_string = "STATUS_DLL_INIT_FAILED";
      break;
    case 1073741510:
      error_string = "STATUS_CONTROL_C_EXIT";
      break;
    case 1073741515:
      error_string = "STATUS_DLL_NOT_FOUND";
      break;
    case 1073741571:
      error_string = "STATUS_STACK_OVERFLOW";
      break;
    case 1073741659:
      error_string = "STATUS_BAD_IMPERSONATION_LEVEL";
      break;
    case 1073741674:
      error_string = "STATUS_PRIVILEGED_INSTRUCTION";
      break;
    case 1073741675:
      error_string = "STATUS_INTEGER_OVERFLOW";
      break;
    case 1073741676:
      error_string = "STATUS_INTEGER_DIVIDE_BY_ZERO";
      break;
    case 1073741677:
      error_string = "STATUS_FLOAT_UNDERFLOW";
      break;
    case 1073741678:
      error_string = "STATUS_FLOAT_STACK_CHECK";
      break;
    case 1073741679:
      error_string = "STATUS_FLOAT_OVERFLOW";
      break;
    case 1073741680:
      error_string = "STATUS_FLOAT_INVALID_OPERATION";
      break;
    case 1073741681:
      error_string = "STATUS_FLOAT_INEXACT_RESULT";
      break;
    case 1073741682:
      error_string = "STATUS_FLOAT_DIVIDE_BY_ZERO";
      break;
    case 1073741683:
      error_string = "STATUS_FLOAT_DENORMAL_OPERAND";
      break;
    case 1073741684:
      error_string = "STATUS_ARRAY_BOUNDS_EXCEEDED";
      break;
    case 1073741783:
      error_string = "STATUS_INVALID_UNWIND_TARGET";
      break;
    case 1073741786:
      error_string = "STATUS_INVALID_DISPOSITION";
      break;
    case 1073741787:
      error_string = "STATUS_NONCONTINUABLE_EXCEPTION";
      break;
    case 1073741790:
      error_string = "STATUS_ACCESS_DENIED";
      break;
    case 1073741794:
      error_string = "STATUS_INVALID_LOCK_SEQUENCE";
      break;
    case 1073741795:
      error_string = "STATUS_ILLEGAL_INSTRUCTION";
      break;
    case 1073741800:
      error_string = "STATUS_CONFLICTING_ADDRESSES";
      break;
    case 1073741801:
      error_string = "STATUS_NO_MEMORY";
      break;
    case 1073741811:
      error_string = "STATUS_INVALID_PARAMETER";
      break;
    case 1073741816:
      error_string = "STATUS_INVALID_HANDLE";
      break;
    case 1073741818:
      error_string = "STATUS_IN_PAGE_ERROR";
      break;
    case 1073741819:
      error_string = "STATUS_ACCESS_VIOLATION";
      break;
    case 1073741829:
      error_string = "STATUS_SEGMENT_NOTIFICATION";
      break;
    case 1073741845:
      error_string = "STATUS_FATAL_APP_EXIT";
      break;
    case 1072103400:
      error_string = "STATUS_CURRENT_TRANSACTION_NOT_VALID";
      break;
    case 1072365548:
      error_string = "STATUS_SXS_CORRUPT_ACTIVATION_STACK";
      break;
    case 1072365552:
      error_string = "STATUS_SXS_INVALID_DEACTIVATION";
      break;
    case 1072365566:
      error_string = "STATUS_SXS_CANT_GEN_ACTCTX";
      break;
    case 1073739514:
      error_string = "STATUS_VIRUS_INFECTED";
      break;
    case 1073740004:
      error_string = "STATUS_INVALID_THREAD";
      break;
    case 1073740016:
      error_string = "STATUS_CALLBACK_RETURNED_WHILE_IMPERSONATING";
      break;
    case 1073740022:
      error_string = "STATUS_THREADPOOL_HANDLE_EXCEPTION";
      break;
    case 1073740767:
      error_string = "STATUS_VERIFIER_STOP";
      break;
    case 1073740768:
      error_string = "STATUS_ASSERTION_FAILURE";
      break;
    case 1073740771:
      error_string = "STATUS_FATAL_USER_CALLBACK_EXCEPTION";
      break;
    case 1073740777:
      error_string = "STATUS_INVALID_CRUNTIME_PARAMETER";
      break;
    case 1073740782:
      error_string = "STATUS_DELAY_LOAD_FAILED";
      break;
    case 1073740959:
      error_string = "STATUS_ACCESS_DISABLED_BY_POLICY_DEFAULT";
      break;
    case 1073741131:
      error_string = "STATUS_FLOAT_MULTIPLE_TRAPS";
      break;
    case 1073741132:
      error_string = "STATUS_FLOAT_MULTIPLE_FAULTS";
      break;
    case 1073741205:
      error_string = "STATUS_DLL_INIT_FAILED_LOGOFF";
      break;
    case 1073741212:
      error_string = "STATUS_RESOURCE_NOT_OWNED";
      break;
    case 1073741431:
      error_string = "STATUS_TOO_LATE";
      break;
    case 1073741511:
      error_string = "STATUS_ENTRYPOINT_NOT_FOUND";
      break;
    case 1073741523:
      error_string = "STATUS_COMMITMENT_LIMIT";
      break;
    case 1073741558:
      error_string = "STATUS_PROCESS_IS_TERMINATING";
      break;
    case 1073741569:
      error_string = "STATUS_BAD_FUNCTION_TABLE";
      break;
    case 1073741581:
      error_string = "STATUS_INVALID_PARAMETER_5";
      break;
    case 1073741595:
      error_string = "STATUS_INTERNAL_ERROR";
      break;
    case 1073741662:
      error_string = "STATUS_MEDIA_WRITE_PROTECTED";
      break;
    case 1073741670:
      error_string = "STATUS_INSUFFICIENT_RESOURCES";
      break;
    case 1073741701:
      error_string = "STATUS_INVALID_IMAGE_FORMAT";
      break;
    case 1073741738:
      error_string = "STATUS_DELETE_PENDING";
      break;
    case 1073741744:
      error_string = "STATUS_EA_TOO_LARGE";
      break;
    case 1073741749:
      error_string = "STATUS_THREAD_IS_TERMINATING";
      break;
    case 1073741756:
      error_string = "STATUS_QUOTA_EXCEEDED";
      break;
    case 1073741757:
      error_string = "STATUS_SHARING_VIOLATION";
      break;
    case 1073741766:
      error_string = "STATUS_OBJECT_PATH_NOT_FOUND";
      break;
    case 1073741772:
      error_string = "STATUS_OBJECT_NAME_NOT_FOUND";
      break;
    case 1073741784:
      error_string = "STATUS_BAD_STACK";
      break;
    case 1073741785:
      error_string = "STATUS_UNWIND";
      break;
    case 1073741788:
      error_string = "STATUS_OBJECT_TYPE_MISMATCH";
      break;
    case 1073741796:
      error_string = "STATUS_INVALID_SYSTEM_SERVICE";
      break;
    case 1073741820:
      error_string = "STATUS_INFO_LENGTH_MISMATCH";
      break;
    case 1073741822:
      error_string = "STATUS_NOT_IMPLEMENTED";
      break;
    case 1073741823:
      error_string = "STATUS_UNSUCCESSFUL";
      break;
    case 2147483644:
      error_string = "STATUS_SINGLE_STEP";
      break;
    case 2147483645:
      error_string = "STATUS_BREAKPOINT";
      break;
    case 2147483646:
      error_string = "STATUS_DATATYPE_MISALIGNMENT";
      break;
    default:
      // Render small error values as integers, and larger values as hex.
      error_string =
          (error_code >= 0 && error_code < 65536)
              ? base::NumberToString(error_code)
              : base::StringPrintf("0x%08lX",
                                   static_cast<unsigned long>(error_code));
  }

  return base::UTF8ToUTF16(error_string);
}

// Show the error code in a selectable label to allow users to copy it.
std::unique_ptr<views::Label> CreateErrorCodeLabel(int format_string,
                                                   int error_code) {
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(format_string, ErrorToString(error_code)),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetSelectable(true);
  label->SetFontList(
      gfx::FontList().Derive(-3, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  return label;
}

}  // namespace

SadTabView::SadTabView(content::WebContents* web_contents, SadTabKind kind)
    : SadTab(web_contents, kind) {
  // This view gets inserted as a child of a WebView, but we don't want the
  // WebView to delete us if the WebView gets deleted before the SadTabHelper
  // does.
  set_owned_by_client();

  SetBackground(views::CreateThemedSolidBackground(ui::kColorDialogBackground));
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::INSETS_DIALOG)));
  auto* top_spacer = AddChildView(std::make_unique<views::View>());
  auto* container = AddChildView(std::make_unique<views::FlexLayoutView>());
  container->SetOrientation(views::LayoutOrientation::kVertical);
  constexpr int kMinContentWidth = 240;
  container->SetMinimumCrossAxisSize(kMinContentWidth);
  auto* bottom_spacer = AddChildView(std::make_unique<views::View>());

  // Center content horizontally; divide vertical padding into 1/3 above, 2/3
  // below.
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->SetFlexForView(top_spacer, 1);
  layout->SetFlexForView(bottom_spacer, 2);

  // Crashed tab image.
  auto* image = container->AddChildView(std::make_unique<views::ImageView>());
  image->SetImage(
      ui::ImageModel::FromVectorIcon(kCrashedTabIcon, ui::kColorIcon, 48));
  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  image->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(0, 0, unrelated_vertical_spacing, 0));
  image->SetProperty(views::kCrossAxisAlignmentKey,
                     views::LayoutAlignment::kStart);

  // Title.
  title_ = container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(GetTitle())));
  title_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::LargeFont));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  constexpr int kTitleBottomSpacing = 13;
  title_->SetProperty(views::kMarginsKey,
                      gfx::Insets::TLBR(0, 0, kTitleBottomSpacing, 0));

  // Message and optional bulleted list.
  message_ = container->AddChildView(
      CreateFormattedLabel(l10n_util::GetStringUTF16(GetInfoMessage())));
  std::vector<int> bullet_string_ids = GetSubMessages();
  if (!bullet_string_ids.empty()) {
    std::vector<std::u16string> texts;
    std::ranges::transform(bullet_string_ids, std::back_inserter(texts),
                           l10n_util::GetStringUTF16);
    auto* list_view =
        container->AddChildView(std::make_unique<views::BulletedLabelListView>(
            std::move(texts), views::style::TextStyle::STYLE_PRIMARY));
    list_view->SetProperty(views::kTableColAndRowSpanKey, gfx::Size(2, 1));
  }

  // Error code.
  container
      ->AddChildView(CreateErrorCodeLabel(GetErrorCodeFormatString(),
                                          GetCrashedErrorCode()))
      ->SetProperty(views::kMarginsKey,
                    gfx::Insets::TLBR(kTitleBottomSpacing, 0,
                                      unrelated_vertical_spacing, 0));

  // Bottom row: help link, action button.
  auto* actions_container =
      container->AddChildView(std::make_unique<views::FlexLayoutView>());
  actions_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  EnableHelpLink(actions_container);

  action_button_ =
      actions_container->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&SadTabView::PerformAction,
                              base::Unretained(this), Action::BUTTON),
          l10n_util::GetStringUTF16(GetButtonTitle())));
  action_button_->SetStyle(ui::ButtonStyle::kProminent);
  action_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(views::LayoutAlignment::kEnd));

  // Needed to ensure this View is drawn even if a sibling (such as dev tools)
  // has a z-order.
  SetPaintToLayer();
  AttachToWebView();

  if (owner_) {
    // If the `owner_` ContentsWebView has a rounded background, the sad tab
    // should also have matching rounded corners as well.
    SetBackgroundRadii(
        static_cast<ContentsWebView*>(owner_)->background_radii());
  }

  // Make the accessibility role of this view an alert dialog, and
  // put focus on the action button. This causes screen readers to
  // immediately announce the text of this view.
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  if (action_button_->GetWidget() && action_button_->GetWidget()->IsActive())
    action_button_->RequestFocus();
}

SadTabView::~SadTabView() {
  if (owner_)
    owner_->SetCrashedOverlayView(nullptr);
}

void SadTabView::ReinstallInWebView() {
  if (owner_) {
    owner_->SetCrashedOverlayView(nullptr);
    owner_ = nullptr;
  }
  AttachToWebView();
}

gfx::RoundedCornersF SadTabView::GetBackgroundRadii() const {
  CHECK(layer());
  return layer()->rounded_corner_radii();
}

void SadTabView::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  // Since SadTabView paints onto its own layer and it is leaf layer, we can
  // round the background by applying rounded corners to the layer without
  // clipping any other browser content.
  CHECK(layer());
  layer()->SetRoundedCornerRadius(radii);
  layer()->SetIsFastRoundedCorner(/*enable=*/true);
}

void SadTabView::OnPaint(gfx::Canvas* canvas) {
  if (!painted_) {
    RecordFirstPaint();
    painted_ = true;
  }
  View::OnPaint(canvas);
}

void SadTabView::RemovedFromWidget() {
  owner_ = nullptr;
}

void SadTabView::AttachToWebView() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  // This can be null during prefetch.
  if (!browser)
    return;

  // In unit tests, browser->window() might not be a real BrowserView.
  if (!browser->window()->GetNativeWindow())
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view);

  views::WebView* web_view = browser_view->contents_web_view();
  if (web_view->GetWebContents() == web_contents()) {
    owner_ = web_view;
    owner_->SetCrashedOverlayView(this);
  }
}

void SadTabView::EnableHelpLink(views::FlexLayoutView* actions_container) {
#if BUILDFLAG(IS_CHROMEOS)
  // Do not show the help link in the kiosk session to prevent escape from a
  // kiosk app.
  if (chromeos::IsKioskSession()) {
    return;
  }
#endif
  auto* help_link =
      actions_container->AddChildView(std::make_unique<views::Link>(
          l10n_util::GetStringUTF16(GetHelpLinkTitle())));
  help_link->SetCallback(base::BindRepeating(
      &SadTab::PerformAction, base::Unretained(this), Action::HELP_LINK));
  help_link->SetProperty(views::kTableVertAlignKey,
                         views::LayoutAlignment::kCenter);
}

void SadTabView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Specify the maximum message and title width explicitly.
  constexpr int kMaxContentWidth = 600;
  const int max_width =
      std::min(width() - ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL) *
                             2,
               kMaxContentWidth);

  message_->SizeToFit(max_width);
  title_->SizeToFit(max_width);
}

SadTab* SadTab::Create(content::WebContents* web_contents, SadTabKind kind) {
  return new SadTabView(web_contents, kind);
}

BEGIN_METADATA(SadTabView)
END_METADATA
