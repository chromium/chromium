#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_DINO_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_DINO_BUTTON_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"

class Browser;

////////////////////////////////////////////////////////////////////////////////
//
// DinoButton
//
// The dino button in the toolbar, which opens chrome://dino
//
////////////////////////////////////////////////////////////////////////////////

class DinoButton : public ToolbarButton,
                   public views::ButtonListener,
                   public ui::SimpleMenuModel::Delegate {
 public:
  // The button's class name.
  static const char kViewClassName[];

  explicit DinoButton(Browser* browser);
  DinoButton(const DinoButton&) = delete;
  DinoButton& operator=(const DinoButton&) = delete;
  ~DinoButton() override;

  void UpdateIcon();

  // views::View:
  void OnThemeChanged() override;

  // ToolbarButton:
  void OnMouseExited(const ui::MouseEvent& event) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool ShouldShowMenu() override;
  void ShowDropDownMenu(ui::MenuSourceType source_type) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* /* button */,
                     const ui::Event& event) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  Browser* browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_DINO_BUTTON_H_
