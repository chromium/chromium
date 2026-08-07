// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_PORTAL_H_
#define CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_PORTAL_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/utils/variant.h"
#include "content/public/browser/eye_dropper.h"

namespace dbus {
class Bus;
}

namespace dbus_xdg {
class Request;
enum class ResponseError;
using Dictionary = std::map<std::string, dbus_utils::Variant>;
using Results = base::expected<Dictionary, ResponseError>;
}  // namespace dbus_xdg

namespace content {
class EyeDropperListener;
class RenderFrameHost;
}  // namespace content

// Eye dropper implementation using the XDG desktop portal.
class EyeDropperPortal : public content::EyeDropper {
 public:
  static std::unique_ptr<content::EyeDropper> Create(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener);

  // For testing.
  static std::unique_ptr<content::EyeDropper> CreateForTesting(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener,
      scoped_refptr<dbus::Bus> bus);

  EyeDropperPortal(const EyeDropperPortal&) = delete;
  EyeDropperPortal& operator=(const EyeDropperPortal&) = delete;

  ~EyeDropperPortal() override;

 private:
  EyeDropperPortal(content::RenderFrameHost* frame,
                   content::EyeDropperListener* listener,
                   scoped_refptr<dbus::Bus> bus);

  void OnWindowHandleExported(std::string handle);
  void OnPortalServiceStarted(uint32_t version);
  void OnResponse(dbus_xdg::Results results);

  const raw_ptr<content::EyeDropperListener> listener_;
  const scoped_refptr<dbus::Bus> bus_;
  std::string parent_handle_;
  std::unique_ptr<dbus_xdg::Request> request_;
  base::WeakPtrFactory<EyeDropperPortal> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_PORTAL_H_
