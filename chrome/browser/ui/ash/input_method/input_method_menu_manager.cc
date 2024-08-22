// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"

#include <stddef.h>

#include "base/memory/singleton.h"

namespace ui {
namespace ime {

InputMethodMenuManager::InputMethodMenuManager() : menu_list_(), observers_() {}

InputMethodMenuManager::~InputMethodMenuManager() {}

void InputMethodMenuManager::AddObserver(
    InputMethodMenuManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void InputMethodMenuManager::RemoveObserver(
    InputMethodMenuManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

InputMethodMenuItemList
InputMethodMenuManager::GetCurrentInputMethodMenuItemList() const {
  return menu_list_;
}

void InputMethodMenuManager::SetCurrentInputMethodMenuItemList(
    const InputMethodMenuItemList& menu_list) {
  menu_list_ = menu_list;
  for (InputMethodMenuManager::Observer& observer : observers_) {
    observer.InputMethodMenuItemChanged(this);
  }
}

bool InputMethodMenuManager::HasInputMethodMenuItemForKey(
    const std::string& key) const {
  for (size_t i = 0; i < menu_list_.size(); ++i) {
    if (menu_list_[i].key == key) {
      return true;
    }
  }
  return false;
}

// static
InputMethodMenuManager* InputMethodMenuManager::GetInstance() {
  return base::Singleton<InputMethodMenuManager>::get();
}

}  // namespace ime
}  // namespace ui
