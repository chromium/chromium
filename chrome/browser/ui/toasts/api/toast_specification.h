// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_API_TOAST_SPECIFICATION_H_
#define CHROME_BROWSER_UI_TOASTS_API_TOAST_SPECIFICATION_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/vector_icon_types.h"

// ToastSpecification details what the toast should contain when shown.
class ToastSpecification {
 public:
  class Builder final {
   public:
    Builder(const gfx::VectorIcon& icon, int body_string_id);
    Builder(const Builder& other) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    // Adds a "X" close button to the toast. However, if the toast have a menu,
    // the close button will be a rounded dismiss button instead.
    Builder& AddCloseButton();

    // Adds a rounded action button toast with the `action_button_string_id`
    // for the label. All toasts with an action button must also have a close
    // button.
    Builder& AddActionButton(int action_button_string_id,
                             base::RepeatingClosure closure);

    // Adds a three dot menu to the toast. Toasts with an action button are not
    // allowed to have menu because they must have an "X" close button instead.
    Builder& AddMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model);

    // Toasts by default are scoped to the active tab when they are triggered.
    // Globally scoped toasts will not immediately dismiss when the user
    // switches tabs or navigates to another site and will rely on timing out to
    // dismiss.
    Builder& AddGlobalScoped();

    // Toast should only dismiss when explicitly instructed to by feature.
    // There can only be one persistent toast shown at a time.
    Builder& AddPersistance();

    std::unique_ptr<ToastSpecification> Build();

   private:
    void ValidateSpecification();

    std::unique_ptr<ToastSpecification> toast_specification_;
  };

  ToastSpecification(base::PassKey<ToastSpecification::Builder>,
                     const gfx::VectorIcon& icon,
                     int string_id);
  ~ToastSpecification();

  int body_string_id() const { return body_string_id_; }
  const gfx::VectorIcon& icon() const { return *icon_; }
  bool has_close_button() const { return has_close_button_; }
  std::optional<int> action_button_string_id() const {
    return action_button_string_id_;
  }
  base::RepeatingClosure action_button_callback() const {
    return action_button_closure_;
  }
  ui::SimpleMenuModel* menu_model() const { return menu_model_.get(); }
  bool is_global_scope() const { return is_global_scope_; }
  bool is_persistent_toast() const { return is_persistent_toast_; }

  void AddCloseButton();
  void AddActionButton(int string_id, base::RepeatingClosure closure);
  void AddMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model);
  void AddGlobalScope();
  void AddPersistance();

 private:
  const base::raw_ref<const gfx::VectorIcon> icon_;
  int body_string_id_;
  bool has_close_button_ = false;
  std::optional<int> action_button_string_id_;
  base::RepeatingClosure action_button_closure_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  bool is_global_scope_ = false;
  bool is_persistent_toast_ = false;
};

#endif  // CHROME_BROWSER_UI_TOASTS_API_TOAST_SPECIFICATION_H_
