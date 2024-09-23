// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MAIN_MENU_BUILDER_H_
#define CHROME_BROWSER_UI_COCOA_MAIN_MENU_BUILDER_H_

#import <Cocoa/Cocoa.h>

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/check_op.h"

namespace chrome {

// Creates the main menu bar using the name specified in |product_name|, and
// registers it as such on |nsapp|. The NSApplicationDelegate |app_delegate|
// is the target for specific, special menu items.
//
//
// Normally the main menu is built in a MainMenu.nib file, but NIB files files
// are hard to edit (especially cross-platform) and bring in a compile
// dependency on ibtool. Building the menu in code has a lower maintenance
// burden.
NSMenu* BuildMainMenu(NSApplication* nsapp,
                      id<NSApplicationDelegate> app_delegate,
                      const std::u16string& product_name,
                      bool is_pwa);

NSMenuItem* BuildFileMenuForTesting(bool is_pwa);

// Internal ////////////////////////////////////////////////////////////////////

namespace internal {

// Helper class that builds NSMenuItems from data.
//
// This builder follows a fluent-interface pattern where the setters are
// not prefixed with the typical "set_" and they return a reference to this
// for easier method chaining.
//
// This class is only exposed for testing.
class MenuItemBuilder {
 public:
  explicit MenuItemBuilder(int string_id = 0);

  MenuItemBuilder(const MenuItemBuilder&);
  MenuItemBuilder& operator=(const MenuItemBuilder&);

  ~MenuItemBuilder();

  // Converts the item to a separator. Only tag() and hidden() are also
  // applicable.
  MenuItemBuilder& is_separator() {
    DCHECK_EQ(string_id_, 0);
    is_separator_ = true;
    return *this;
  }

  MenuItemBuilder& target(id target) {
    target_ = target;
    return *this;
  }

  MenuItemBuilder& action(SEL action) {
    DCHECK(!action_);
    action_ = action;
    return *this;
  }

  MenuItemBuilder& tag(int tag) {
    tag_ = tag;
    return *this;
  }

  // Wires up the menu item to the CommandDispatcher based on an
  // IDC_ command code.
  MenuItemBuilder& command_id(int command_id) {
    return tag(command_id).action(@selector(commandDispatch:));
  }

  // Specifies the string to substitute for the $1 found in the string for
  // |string_id_|.
  MenuItemBuilder& string_format_1(const std::u16string& arg) {
    string_arg1_ = arg;
    return *this;
  }

  MenuItemBuilder& submenu(std::vector<MenuItemBuilder> items) {
    submenu_ = std::move(items);
    return *this;
  }

  // Registers a custom key equivalent. Normally the key equivalent is looked
  // up via AcceleratorsCocoa based on the command_id(). If one is not present,
  // the one specified here is used instead.
  MenuItemBuilder& key_equivalent(NSString* key_equivalent,
                                  NSEventModifierFlags flags) {
    CHECK((flags & NSEventModifierFlagShift) == 0)
        << "The shift modifier flag should be directly applied to the key "
           "equivalent.";
    key_equivalent_ = key_equivalent;
    key_equivalent_flags_ = flags;
    return *this;
  }

  // Marks the item as an alternate keyboard equivalent menu item.
  MenuItemBuilder& is_alternate() {
    is_alternate_ = true;
    return *this;
  }

  // Excludes this item from the menu if |condition| is true.
  MenuItemBuilder& remove_if(bool condition) {
    is_removed_ |= condition;
    return *this;
  }

  // Hide this item from the menu if |condition| is true.
  MenuItemBuilder& set_hidden(bool condition) {
    is_hidden_ |= condition;
    return *this;
  }

  // Marks the item as a section header menu item.
  MenuItemBuilder& is_section_header() {
    is_section_header_ = true;
    return *this;
  }

  // Builds a NSMenuItem instance from the properties set on the Builder.
  NSMenuItem* Build() const;

 private:
  bool is_separator_ = false;

  int string_id_ = 0;
  std::u16string string_arg1_;

  int tag_ = 0;

  id __strong target_ = nil;
  SEL action_ = nil;

  NSString* __strong key_equivalent_ = @"";
  NSEventModifierFlags key_equivalent_flags_ = 0;

  bool is_alternate_ = false;

  bool is_removed_ = false;

  std::optional<std::vector<MenuItemBuilder>> submenu_;

  bool is_hidden_ = false;

  bool is_section_header_ = false;
};

}  // namespace internal
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_MAIN_MENU_BUILDER_H_
