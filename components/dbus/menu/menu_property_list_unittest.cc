// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/menu/menu_property_list.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/dbus/properties/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

class TestMenuModel : public ui::SimpleMenuModel,
                      public ui::SimpleMenuModel::Delegate {
 public:
  TestMenuModel(bool checked,
                bool enabled,
                bool visible,
                const std::u16string& label,
                const gfx::Image& icon,
                const ui::Accelerator& accelerator)
      : ui::SimpleMenuModel(this),
        checked_(checked),
        enabled_(enabled),
        visible_(visible),
        label_(label),
        icon_(icon),
        accelerator_(accelerator) {}
  ~TestMenuModel() override = default;

  MenuItemProperties ComputeProperties() {
    return ComputeMenuPropertiesForMenuItem(this, 0);
  }

 protected:
  // ui::MenuModel::
  bool IsItemDynamicAt(size_t index) const override {
    EXPECT_EQ(index, 0u);
    // Return true so that GetIconForCommandId() will always be called.
    return true;
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    EXPECT_LE(command_id, 0);
    return checked_;
  }
  bool IsCommandIdEnabled(int command_id) const override {
    EXPECT_LE(command_id, 0);
    return enabled_;
  }
  bool IsCommandIdVisible(int command_id) const override {
    EXPECT_LE(command_id, 0);
    return visible_;
  }
  std::u16string GetLabelForCommandId(int command_id) const override {
    EXPECT_LE(command_id, 0);
    return label_;
  }
  ui::ImageModel GetIconForCommandId(int command_id) const override {
    EXPECT_LE(command_id, 0);
    return icon_.IsEmpty() ? ui::ImageModel()
                           : ui::ImageModel::FromImage(icon_);
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    EXPECT_LE(command_id, 0);
  }
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    EXPECT_LE(command_id, 0);
    if (accelerator_ == ui::Accelerator())
      return false;
    *accelerator = accelerator_;
    return true;
  }

 private:
  const bool checked_;
  const bool enabled_;
  const bool visible_;
  const std::u16string label_;
  const gfx::Image icon_;
  const ui::Accelerator accelerator_;
};

class TestMenuModelBuilder {
 public:
  TestMenuModelBuilder() = default;
  ~TestMenuModelBuilder() = default;

  TestMenuModelBuilder SetType(ui::MenuModel::ItemType type) const {
    TestMenuModelBuilder builder = *this;
    builder.type_ = type;
    return builder;
  }

  TestMenuModelBuilder SetChecked(bool checked) const {
    TestMenuModelBuilder builder = *this;
    builder.checked_ = checked;
    return builder;
  }

  TestMenuModelBuilder SetEnabled(bool enabled) const {
    TestMenuModelBuilder builder = *this;
    builder.enabled_ = enabled;
    return builder;
  }

  TestMenuModelBuilder SetVisible(bool visible) const {
    TestMenuModelBuilder builder = *this;
    builder.visible_ = visible;
    return builder;
  }

  TestMenuModelBuilder SetLabel(const std::string& label) const {
    TestMenuModelBuilder builder = *this;
    builder.label_ = base::ASCIIToUTF16(label);
    return builder;
  }

  TestMenuModelBuilder SetIcon(const gfx::Image& icon) const {
    TestMenuModelBuilder builder = *this;
    builder.icon_ = icon;
    return builder;
  }

  TestMenuModelBuilder SetAccelerator(
      const ui::Accelerator& accelerator) const {
    TestMenuModelBuilder builder = *this;
    builder.accelerator_ = accelerator;
    return builder;
  }

  std::unique_ptr<TestMenuModel> Build() const {
    auto menu = std::make_unique<TestMenuModel>(checked_, enabled_, visible_,
                                                label_, icon_, accelerator_);
    switch (type_) {
      case ui::MenuModel::TYPE_COMMAND:
        menu->AddItem(0, label_);
        break;
      case ui::MenuModel::TYPE_TITLE:
        menu->AddTitle(label_);
        break;
      case ui::MenuModel::TYPE_CHECK:
        menu->AddCheckItem(0, label_);
        break;
      case ui::MenuModel::TYPE_RADIO:
        menu->AddRadioItem(0, label_, 0);
        break;
      case ui::MenuModel::TYPE_SEPARATOR:
        menu->AddSeparator(ui::MenuSeparatorType::SPACING_SEPARATOR);
        break;
      case ui::MenuModel::TYPE_BUTTON_ITEM:
        NOTIMPLEMENTED();
        break;
      case ui::MenuModel::TYPE_SUBMENU:
        menu->AddSubMenu(0, label_, nullptr);
        break;
      case ui::MenuModel::TYPE_ACTIONABLE_SUBMENU:
        menu->AddActionableSubMenu(0, label_, nullptr);
        break;
      case ui::MenuModel::TYPE_HIGHLIGHTED:
        menu->AddHighlightedItemWithIcon(0, label_,
                                         ui::ImageModel::FromImage(icon_));
        break;
    }
    return menu;
  }

 private:
  ui::MenuModel::ItemType type_ = ui::MenuModel::TYPE_COMMAND;
  bool checked_ = false;
  bool enabled_ = true;
  bool visible_ = true;
  std::u16string label_;
  gfx::Image icon_;
  ui::Accelerator accelerator_;
};

}  // namespace

TEST(MenuPropertyListTest, ComputePropertiesBasic) {
  auto builder = TestMenuModelBuilder();
  auto menu = builder.Build();
  MenuItemProperties props;
  EXPECT_EQ(menu->ComputeProperties(), props);

  // Same for TYPE_HIGHLIGHTED.
  menu = builder.SetType(ui::MenuModel::TYPE_HIGHLIGHTED).Build();
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesCheck) {
  auto menu = TestMenuModelBuilder().SetType(ui::MenuModel::TYPE_CHECK).Build();
  MenuItemProperties props;
  props["toggle-type"] = MakeDbusVariant(DbusString("checkmark"));
  props["toggle-state"] = MakeDbusVariant(DbusInt32(0));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesRadio) {
  auto menu = TestMenuModelBuilder().SetType(ui::MenuModel::TYPE_RADIO).Build();
  MenuItemProperties props;
  props["toggle-type"] = MakeDbusVariant(DbusString("radio"));
  props["toggle-state"] = MakeDbusVariant(DbusInt32(0));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesCheckedState) {
  auto builder = TestMenuModelBuilder().SetChecked(true);

  // Types other than radio and check should not have toggle-state set.
  auto menu = builder.Build();
  MenuItemProperties props;
  EXPECT_EQ(menu->ComputeProperties(), props);

  // Radio and check buttons should have the toggle-state set.
  menu = builder.SetType(ui::MenuModel::TYPE_RADIO).Build();
  props["toggle-type"] = MakeDbusVariant(DbusString("radio"));
  props["toggle-state"] = MakeDbusVariant(DbusInt32(1));
  EXPECT_EQ(menu->ComputeProperties(), props);

  menu = builder.SetType(ui::MenuModel::TYPE_CHECK).Build();
  props["toggle-type"] = MakeDbusVariant(DbusString("checkmark"));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesSeparator) {
  auto menu =
      TestMenuModelBuilder().SetType(ui::MenuModel::TYPE_SEPARATOR).Build();
  MenuItemProperties props;
  props["type"] = MakeDbusVariant(DbusString("separator"));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesSubmenu) {
  auto builder = TestMenuModelBuilder();
  auto menu = builder.SetType(ui::MenuModel::TYPE_SUBMENU).Build();
  MenuItemProperties props;
  props["children-display"] = MakeDbusVariant(DbusString("submenu"));
  EXPECT_EQ(menu->ComputeProperties(), props);

  // Same for ACTIONABLE_SUBMENU.
  menu = builder.SetType(ui::MenuModel::TYPE_ACTIONABLE_SUBMENU).Build();
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesEnabledState) {
  auto builder = TestMenuModelBuilder();

  // Enabled.
  auto menu = builder.SetEnabled(true).Build();
  MenuItemProperties props;
  EXPECT_EQ(menu->ComputeProperties(), props);

  // Disabled.
  menu = builder.SetEnabled(false).Build();
  props["enabled"] = MakeDbusVariant(DbusBoolean(false));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesVisibleState) {
  auto builder = TestMenuModelBuilder();

  // Visible.
  auto menu = builder.SetVisible(true).Build();
  MenuItemProperties props;
  EXPECT_EQ(menu->ComputeProperties(), props);

  // Hidden.
  menu = builder.SetVisible(false).Build();
  props["visible"] = MakeDbusVariant(DbusBoolean(false));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesLabel) {
  auto builder = TestMenuModelBuilder();

  // No label.
  auto menu = builder.SetLabel("").Build();
  MenuItemProperties props;
  EXPECT_EQ(menu->ComputeProperties(), props);

  // Non-empty label.
  menu = builder.SetLabel("label value").Build();
  props["label"] = MakeDbusVariant(DbusString("label value"));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

TEST(MenuPropertyListTest, ComputePropertiesIcon) {
  auto builder = TestMenuModelBuilder();

  // No icon.
  auto menu = builder.SetIcon(gfx::Image()).Build();
  MenuItemProperties props;
  EXPECT_EQ(menu->ComputeProperties(), props);

  // Non-empty label.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseARGB(255, 123, 123, 123);
  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, 1.0f));
  gfx::Image icon(image_skia);
  menu = builder.SetIcon(icon).Build();
  props["icon-data"] = MakeDbusVariant(DbusByteArray(icon.As1xPNGBytes()));
  EXPECT_EQ(menu->ComputeProperties(), props);
}

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
TEST(MenuPropertyListTest, ComputePropertiesAccelerator) {
  // The Wayland implementation requires the keyboard layout to be set.
  // The ScopedKeyboardLayout does not unset the already existing layout engine,
  // so we do so here and restore in the end of the test.
  auto* const old_layout =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  ui::KeyboardLayoutEngineManager::ResetKeyboardLayoutEngine();

  {
    ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

    auto builder = TestMenuModelBuilder();

    // No accelerator.
    auto menu = builder.SetAccelerator(ui::Accelerator()).Build();
    MenuItemProperties props;
    EXPECT_EQ(menu->ComputeProperties(), props);

    // Set a key.
    menu = builder.SetAccelerator(ui::Accelerator(ui::VKEY_A, 0)).Build();
    props["shortcut"] =
        MakeDbusVariant(MakeDbusArray(MakeDbusArray(DbusString("a"))));
    EXPECT_EQ(menu->ComputeProperties(), props);

    // Add modifiers.
    menu = builder
               .SetAccelerator(ui::Accelerator(
                   ui::VKEY_A,
                   ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN))
               .Build();
    props["shortcut"] = MakeDbusVariant(
        MakeDbusArray(MakeDbusArray(DbusString("Control"), DbusString("Alt"),
                                    DbusString("Shift"), DbusString("a"))));
    EXPECT_EQ(menu->ComputeProperties(), props);
  }

  if (old_layout)
    ui::KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(old_layout);
}
#endif  // BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)

TEST(MenuPropertyListTest, ComputePropertyChanges) {
  MenuItemProperties old_props;
  old_props["1"] = MakeDbusVariant(DbusInt32(1));  // Remains the same.
  old_props["2"] = MakeDbusVariant(DbusInt32(2));  // Updates to -2.
  old_props["3"] = MakeDbusVariant(DbusInt32(3));  // Removed.

  MenuItemProperties new_props;
  new_props["1"] = MakeDbusVariant(DbusInt32(1));
  new_props["2"] = MakeDbusVariant(DbusInt32(-2));
  new_props["4"] = MakeDbusVariant(DbusInt32(4));  // Added.

  MenuPropertyList updated;
  MenuPropertyList removed;
  ComputeMenuPropertyChanges(old_props, new_props, &updated, &removed);
  EXPECT_EQ(updated, (MenuPropertyList{"2", "4"}));
  EXPECT_EQ(removed, (MenuPropertyList{"3"}));
}
