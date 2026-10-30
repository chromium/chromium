// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/background_portal.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"

namespace dbus_xdg {

namespace {

constexpr char kBackgroundInterfaceName[] = "org.freedesktop.portal.Background";

// Maximum length in UTF-8 code points accepted by
// org.freedesktop.portal.Background.SetStatus (`g_utf8_strlen(message, -1)`).
constexpr size_t kMaxStatusMessageCodePoints = 96;

std::string_view TruncateToUTF8CodePoints(std::string_view input,
                                          size_t max_code_points) {
  size_t byte_end = 0;
  size_t count = 0;
  for (size_t index = 0; count < max_code_points && index < input.size();
       ++count, ++index) {
    base_icu::UChar32 code_point = 0;
    if (!base::ReadUnicodeCharacter(input, &index, &code_point) ||
        !base::IsValidCharacter(code_point)) {
      break;
    }
    byte_end = index + 1;
  }
  return input.substr(0, byte_end);
}

void OnRequestResponse(BackgroundResultCallback callback,
                       base::expected<Dictionary, ResponseError> dict_result) {
  if (!dict_result.has_value()) {
    std::move(callback).Run(base::unexpected(dict_result.error()));
    return;
  }

  Dictionary dict = std::move(dict_result).value();

  // "background" is required and must be a boolean.
  auto bg_it = dict.find("background");
  if (bg_it == dict.end()) {
    std::move(callback).Run(
        base::unexpected(ResponseError::kInvalidSignalResponse));
    return;
  }

  auto bg_val = std::move(bg_it->second).Take<bool>();
  if (!bg_val.has_value()) {
    std::move(callback).Run(
        base::unexpected(ResponseError::kInvalidSignalResponse));
    return;
  }

  BackgroundResult result;
  result.background_allowed = *bg_val;

  // "autostart" is optional, but must be a boolean if present.
  if (auto auto_it = dict.find("autostart"); auto_it != dict.end()) {
    auto auto_val = std::move(auto_it->second).Take<bool>();
    if (!auto_val.has_value()) {
      std::move(callback).Run(
          base::unexpected(ResponseError::kInvalidSignalResponse));
      return;
    }
    result.autostart_enabled = *auto_val;
  }

  std::move(callback).Run(result);
}

}  // namespace

BackgroundOptions::BackgroundOptions() = default;
BackgroundOptions::BackgroundOptions(const BackgroundOptions&) = default;
BackgroundOptions& BackgroundOptions::operator=(const BackgroundOptions&) =
    default;
BackgroundOptions::BackgroundOptions(BackgroundOptions&&) = default;
BackgroundOptions& BackgroundOptions::operator=(BackgroundOptions&&) = default;
BackgroundOptions::~BackgroundOptions() = default;

BackgroundPortal::BackgroundPortal()
    : BackgroundPortal(dbus_thread_linux::GetSharedSessionBus()) {}

BackgroundPortal::BackgroundPortal(scoped_refptr<dbus::Bus> bus)
    : bus_(std::move(bus)) {}

BackgroundPortal::~BackgroundPortal() = default;

void BackgroundPortal::RequestBackground(const std::string& parent_window,
                                         const BackgroundOptions& options,
                                         BackgroundResultCallback callback) {
  // Reset `active_request_` before starting a new request so `Close()` for any
  // in-flight request is queued on the D-Bus thread before the new
  // `RequestBackground` call.
  const uint64_t generation = ++request_generation_;
  active_request_.reset();

  if (!bus_) {
    if (callback) {
      std::move(callback).Run(
          base::unexpected(ResponseError::kMethodCallFailed));
    }
    return;
  }

  RequestXdgDesktopPortal(
      bus_.get(), kBackgroundInterfaceName,
      base::BindOnce(&BackgroundPortal::OnPortalReadyForRequestBackground,
                     weak_ptr_factory_.GetWeakPtr(), generation, parent_window,
                     options, std::move(callback)));
}

void BackgroundPortal::SetStatus(const std::string& message,
                                 SetStatusCallback callback) {
  if (!bus_) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }

  RequestXdgDesktopPortal(
      bus_.get(), kBackgroundInterfaceName,
      base::BindOnce(&BackgroundPortal::OnPortalReadyForSetStatus,
                     weak_ptr_factory_.GetWeakPtr(), message,
                     std::move(callback)));
}

void BackgroundPortal::OnPortalReadyForRequestBackground(
    uint64_t generation,
    const std::string& parent_window,
    const BackgroundOptions& options,
    BackgroundResultCallback callback,
    uint32_t version) {
  if (generation != request_generation_) {
    return;
  }

  if (version < 1) {
    if (callback) {
      std::move(callback).Run(
          base::unexpected(ResponseError::kMethodCallFailed));
    }
    return;
  }

  Dictionary portal_options;
  if (!options.reason.empty()) {
    portal_options["reason"] = dbus_utils::Variant::Wrap<"s">(options.reason);
  }
  portal_options["autostart"] =
      dbus_utils::Variant::Wrap<"b">(options.autostart);
  if (!options.commandline.empty()) {
    portal_options["commandline"] =
        dbus_utils::Variant::Wrap<"as">(options.commandline);
  }
  portal_options["dbus-activatable"] =
      dbus_utils::Variant::Wrap<"b">(options.dbus_activatable);

  auto* proxy = bus_->GetObjectProxy(kPortalServiceName,
                                     dbus::ObjectPath(kPortalObjectPath));

  active_request_ = std::make_unique<Request>(
      bus_, proxy, kBackgroundInterfaceName, "RequestBackground",
      std::move(portal_options),
      base::BindOnce(&OnRequestResponse,
                     base::BindOnce(&BackgroundPortal::OnRequestFinished,
                                    weak_ptr_factory_.GetWeakPtr(), generation,
                                    std::move(callback))),
      parent_window);
}

void BackgroundPortal::OnRequestFinished(
    uint64_t generation,
    BackgroundResultCallback callback,
    base::expected<BackgroundResult, ResponseError> result) {
  if (generation != request_generation_) {
    return;
  }
  active_request_.reset();
  if (callback) {
    std::move(callback).Run(result);
  }
}

void BackgroundPortal::OnPortalReadyForSetStatus(const std::string& message,
                                                 SetStatusCallback callback,
                                                 uint32_t version) {
  // SetStatus was added in org.freedesktop.portal.Background version 2.
  if (version < 2) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }

  // The portal requires a single-line message, so replace newlines with
  // spaces.
  std::string clean_message;
  base::ReplaceChars(message, "\r\n", " ", &clean_message);

  // The portal rejects messages longer than 96 UTF-8 code points
  // (`g_utf8_strlen(message, -1) > 96`).
  std::string truncated_message(
      TruncateToUTF8CodePoints(clean_message, kMaxStatusMessageCodePoints));

  Dictionary options;
  options["message"] = dbus_utils::Variant::Wrap<"s">(truncated_message);

  auto* proxy = bus_->GetObjectProxy(kPortalServiceName,
                                     dbus::ObjectPath(kPortalObjectPath));

  dbus_utils::CallMethod<"a{sv}", "">(
      proxy, kBackgroundInterfaceName, "SetStatus",
      base::BindOnce(
          [](SetStatusCallback callback,
             dbus_utils::CallMethodResultSig<""> result) {
            if (callback) {
              std::move(callback).Run(result.has_value());
            }
          },
          std::move(callback)),
      std::move(options));
}

}  // namespace dbus_xdg
