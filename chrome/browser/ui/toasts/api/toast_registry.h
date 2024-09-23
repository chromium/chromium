// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_API_TOAST_REGISTRY_H_
#define CHROME_BROWSER_UI_TOASTS_API_TOAST_REGISTRY_H_

#include <map>
#include <memory>

class ToastSpecification;
enum class ToastId;

// The ToastRegistry keeps track of all registered ToastIds and the
// corresponding ToastSpecifications.
class ToastRegistry {
 public:
  ToastRegistry();
  ~ToastRegistry();

  // Registers `id` with the provided `specification`.
  void RegisterToast(ToastId id,
                     std::unique_ptr<ToastSpecification> specification);

  // Returns whether there are ids currently registered with the registry.
  bool IsEmpty() const;

  // Returns the corresponding ToastSpecification to `id`.
  const ToastSpecification* GetToastSpecification(ToastId id) const;

 private:
  std::map<ToastId, std::unique_ptr<ToastSpecification>> toast_specifications_;
};

#endif  // CHROME_BROWSER_UI_TOASTS_API_TOAST_REGISTRY_H_
