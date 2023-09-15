// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/orca_resources.h"
#include "chrome/grit/orca_resources_map.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

namespace ash {

namespace {

constexpr int kCursorVerticalPadding = 8;

constexpr int kMakoCornerRadius = 20;

// Height threshold of the mako rewrite UI which determines its screen position.
// Tall UI is centered on the display screen containing the caret, while short
// UI is anchored at the caret.
constexpr int kMakoRewriteHeightThreshold = 400;

// TODO(b/289969807): As a placeholder, use 3961 which is the emoji picker
// identifier for task manager. We should create a proper one for mako.
constexpr int kMakoTaskManagerStringID = 3961;

const ui::TextInputClient* GetTextInputClient() {
  const ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  return input_method ? input_method->GetTextInputClient() : nullptr;
}

class MakoRewriteView : public WebUIBubbleDialogView {
 public:
  METADATA_HEADER(MakoRewriteView);
  MakoRewriteView(BubbleContentsWrapper* contents_wrapper,
                  const gfx::Rect& caret_bounds)
      : WebUIBubbleDialogView(nullptr, contents_wrapper),
        caret_bounds_(caret_bounds) {
    set_has_parent(false);
    set_corner_radius(kMakoCornerRadius);
    set_adjust_if_offscreen(true);
  }
  MakoRewriteView(const MakoRewriteView&) = delete;
  MakoRewriteView& operator=(const MakoRewriteView&) = delete;
  ~MakoRewriteView() override = default;

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    if (new_size.height() > kMakoRewriteHeightThreshold) {
      // Place tall UI at the center of the screen.
      SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
      SetAnchorRect(display::Screen::GetScreen()
                        ->GetDisplayMatching(caret_bounds_)
                        .work_area());
    } else {
      // Anchor short UI at the caret.
      SetArrowWithoutResizing(views::BubbleBorder::TOP_LEFT);
      gfx::Rect anchor_rect = caret_bounds_;
      anchor_rect.Outset(gfx::Outsets::VH(kCursorVerticalPadding, 0));
      SetAnchorRect(anchor_rect);
    }
    WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);
  }

 private:
  gfx::Rect caret_bounds_;
};

BEGIN_METADATA(MakoRewriteView, WebUIBubbleDialogView)
END_METADATA

class MakoConsentView : public WebUIBubbleDialogView {
 public:
  METADATA_HEADER(MakoConsentView);
  MakoConsentView(BubbleContentsWrapper* contents_wrapper,
                  const gfx::Rect& caret_bounds)
      : WebUIBubbleDialogView(nullptr, contents_wrapper) {
    set_has_parent(false);
    set_corner_radius(kMakoCornerRadius);
    SetModalType(ui::MODAL_TYPE_SYSTEM);
    SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
    SetAnchorRect(display::Screen::GetScreen()
                      ->GetDisplayMatching(caret_bounds)
                      .work_area());
  }
  MakoConsentView(const MakoConsentView&) = delete;
  MakoConsentView& operator=(const MakoConsentView&) = delete;
  ~MakoConsentView() override = default;
};

BEGIN_METADATA(MakoConsentView, WebUIBubbleDialogView)
END_METADATA

}  // namespace

MakoUntrustedUIConfig::MakoUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme, ash::kChromeUIMakoHost) {}

MakoUntrustedUIConfig::~MakoUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
MakoUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                             const GURL& url) {
  return std::make_unique<MakoUntrustedUI>(web_ui);
}

bool MakoUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return chromeos::features::IsOrcaEnabled();
}

MakoUntrustedUI::MakoUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedBubbleWebUIController(web_ui) {
  CHECK(chromeos::features::IsOrcaEnabled());

  const std::string debug_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kOrcaKey));
  // See go/orca-key for the key
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/auuf123 --orca-key="INSERT KEY
  //  HERE" --enable-features=Orca
  const std::string hash =
      "\x7a\xf3\xa1\x57\x28\x48\xc4\x14\x27\x13\x53\x5a\x09\xf3\x0e\xfc\xee\xa6"
      "\xbb\xa4";
  // If key fails to match, crash chrome.
  CHECK_EQ(debug_key_hash, hash);

  // Setup the data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIMakoURL);
  webui::SetupWebUIDataSource(
      source, base::make_span(kOrcaResources, kOrcaResourcesSize),
      IDR_MAKO_ORCA_HTML);
  source->SetDefaultResource(IDR_MAKO_ORCA_HTML);

  // Setup additional CSP overrides
  // Intentional space at end of the strings - things are appended to this.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types goog#html polymer_resin lit-html "
      "polymer-template-event-attribute-policy polymer-html-literal; ");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'unsafe-inline'; ");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src data:; ");
}
MakoUntrustedUI::~MakoUntrustedUI() = default;

void MakoUntrustedUI::BindInterface(
    mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver) {
  input_method::EditorMediator::Get()->BindEditorClient(
      std::move(pending_receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MakoUntrustedUI)

MakoPageHandler::MakoPageHandler() = default;

MakoPageHandler::~MakoPageHandler() {
  CloseUI();
}

void MakoPageHandler::ShowConsentUI(Profile* profile) {
  if (!GetTextInputClient()) {
    return;
  }

  // TODO(b/300554470): Use the actual consent url, maybe using the constants
  // defined in the orca_resources_map.
  contents_wrapper_ = std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
      GURL(kChromeUIOrcaURL), profile, kMakoTaskManagerStringID);
  contents_wrapper_->ReloadWebContents();
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoConsentView>(contents_wrapper_.get(),
                                        GetTextInputClient()->GetCaretBounds()))
      ->Show();
}

void MakoPageHandler::ShowRewriteUI(Profile* profile) {
  if (!GetTextInputClient()) {
    return;
  }

  contents_wrapper_ = std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
      GURL(kChromeUIOrcaURL), profile, kMakoTaskManagerStringID);
  contents_wrapper_->ReloadWebContents();
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoRewriteView>(contents_wrapper_.get(),
                                        GetTextInputClient()->GetCaretBounds()))
      ->Show();
}

void MakoPageHandler::CloseUI() {
  if (contents_wrapper_) {
    contents_wrapper_->CloseUI();
    contents_wrapper_ = nullptr;
  }
}

}  // namespace ash
