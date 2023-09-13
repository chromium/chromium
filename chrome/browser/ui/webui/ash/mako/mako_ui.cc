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
#include "ui/display/screen.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

namespace {

constexpr int kCursorVerticalPadding = 8;

// Height threshold of the mako UI which determines its screen position. Tall UI
// is centered on the display screen containing the caret, while short UI is
// anchored at the caret.
constexpr int kMakoHeightThreshold = 400;

class MakoDialogView : public WebUIBubbleDialogView {
 public:
  MakoDialogView(std::unique_ptr<BubbleContentsWrapper> contents_wrapper,
                 const gfx::Rect& caret_bounds)
      : WebUIBubbleDialogView(nullptr, contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)),
        caret_bounds_(caret_bounds) {
    set_has_parent(false);
    set_corner_radius(20);
    set_adjust_if_offscreen(true);
  }

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    if (new_size.height() > kMakoHeightThreshold) {
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
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
  gfx::Rect caret_bounds_;
};

}  // namespace

namespace ash {

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
    mojo::PendingReceiver<input_method::mojom::EditorInstance> receiver) {
  input_method::EditorMediator::Get()->BindEditorInstance(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MakoUntrustedUI)

MakoPageHandler::MakoPageHandler() {
  // TODO(b/289859230): Construct MakoUntrustedUI and show it to the user. Save
  //   a ref to the constructed view to allow for closing it at a later time.
  NOTIMPLEMENTED_LOG_ONCE();
}

MakoPageHandler::~MakoPageHandler() = default;

void MakoPageHandler::CloseUI() {
  // TODO(b/289859230): Use the ref saved from construction to close the webui.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MakoUntrustedUI::Show(Profile* profile) {
  // Don't show mako if there is no input client.
  ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  ui::TextInputClient* input_client =
      input_method ? input_method->GetTextInputClient() : nullptr;
  if (!input_client) {
    return;
  }

  // TODO(b/289969807): 3961 is emoji picker identifier for task manager - we
  // should have a better one for mako
  auto contents_wrapper =
      std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
          GURL(kChromeUIOrcaURL), profile, 3961);
  contents_wrapper->ReloadWebContents();

  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<MakoDialogView>(std::move(contents_wrapper),
                                       input_client->GetCaretBounds()))
      ->Show();
}

}  // namespace ash
