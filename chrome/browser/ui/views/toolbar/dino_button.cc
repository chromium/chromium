#include "chrome/browser/ui/views/toolbar/dino_button.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/toolbar/button_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/metrics.h"
#include "ui/views/widget/widget.h"

namespace {

const gfx::VectorIcon& GetIcon() {
  return vector_icons::kMediaNextTrackIcon;
}

}  // namespace

// DinoButton ---------------------------------------------------------------

// static
const char DinoButton::kViewClassName[] = "DinoButton";

DinoButton::DinoButton(Browser* browser)
    : ToolbarButton(this, std::make_unique<ui::SimpleMenuModel>(this), nullptr),
      browser_(browser) {}

DinoButton::~DinoButton() {}


void DinoButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateIcon();
}

void DinoButton::UpdateIcon() {
  // There's no reason to make graphical changes when we're not yet in a
  // Widget.  This function will be called again after widget addition.
  if (!GetWidget())
    return;

  UpdateIconsWithStandardColors(GetIcon());
}

void DinoButton::OnMouseExited(const ui::MouseEvent& event) {
  ToolbarButton::OnMouseExited(event);
}

base::string16 DinoButton::GetTooltipText(const gfx::Point& p) const {
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_DINO);
}

const char* DinoButton::GetClassName() const {
  return kViewClassName;
}

void DinoButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
}

bool DinoButton::ShouldShowMenu() {
  return false;
}

void DinoButton::ShowDropDownMenu(ui::MenuSourceType) {
}

void DinoButton::ButtonPressed(views::Button* /* button */,
                               const ui::Event& event) {
  ClearPendingMenu();

  if (!browser_)
    return;

  GURL url("chrome://dino");
  content::OpenURLParams params(url,
                content::Referrer(url, network::mojom::ReferrerPolicy::kNever),
                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                ui::PageTransition::PAGE_TRANSITION_FIRST,
                false);
  browser_->OpenURL(params);
}

bool DinoButton::IsCommandIdChecked(int command_id) const {
  return false;
}

bool DinoButton::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool DinoButton::IsCommandIdVisible(int command_id) const {
  return true;
}

bool DinoButton::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void DinoButton::ExecuteCommand(int, int) {
  return;
}
