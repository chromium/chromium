// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_install_options.h"

#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace web_app {

ExternalInstallOptions::ExternalInstallOptions(
    const GURL& install_url,
    std::optional<mojom::UserDisplayMode> user_display_mode,
    ExternalInstallSource install_source)
    : install_url(install_url),
      user_display_mode(user_display_mode),
      install_source(install_source) {
  CHECK(install_url.is_valid(), base::NotFatalUntil::M130);
}

ExternalInstallOptions::~ExternalInstallOptions() = default;

ExternalInstallOptions::ExternalInstallOptions(
    const ExternalInstallOptions& other) = default;

ExternalInstallOptions::ExternalInstallOptions(ExternalInstallOptions&& other) =
    default;

ExternalInstallOptions& ExternalInstallOptions::operator=(
    const ExternalInstallOptions& other) = default;

bool ExternalInstallOptions::operator==(
    const ExternalInstallOptions& other) const {
  auto AsTuple = [](const ExternalInstallOptions& options) {
    // Keep in order declared in external_install_options.h.
    return std::tie(
        // clang-format off
        options.install_url,
        options.user_display_mode,
        options.install_source,
        options.fallback_app_name,
        options.add_to_applications_menu,
        options.add_to_desktop,
        options.add_to_quick_launch_bar,
        options.add_to_search,
        options.add_to_management,
        options.is_disabled,
        options.override_previous_user_uninstall,
        options.only_for_new_users,
        options.only_if_previously_preinstalled,
        options.user_type_allowlist,
        options.gate_on_feature,
        options.gate_on_feature_or_installed,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        options.disable_if_arc_supported,
        options.disable_if_tablet_form_factor,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        options.require_manifest,
        options.install_as_shortcut,
        options.is_preferred_app_for_supported_links,
        options.force_reinstall,
        options.force_reinstall_for_milestone,
        options.placeholder_resolution_behavior,
        options.install_placeholder,
        options.launch_query_params,
        options.load_and_await_service_worker_registration,
        options.service_worker_registration_url,
        options.service_worker_registration_timeout,
        options.uninstall_and_replace,
        options.additional_search_terms,
        options.only_use_app_info_factory,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        options.system_app_type,
#endif
        options.oem_installed,
        options.disable_if_touchscreen_with_stylus_not_supported,
        options.handles_file_open_intents,
        options.expected_app_id,
        options.install_without_os_integration
        // clang-format on
    );
  };
  return AsTuple(*this) == AsTuple(other);
}

base::Value ExternalInstallOptions::AsDebugValue() const {
  base::Value::Dict root;

  auto ConvertStringList = [](const std::vector<std::string>& list) {
    base::Value::List list_json;
    for (const std::string& item : list)
      list_json.Append(item);
    return list_json;
  };

  auto ConvertOptional = [](const auto& value) {
    return value ? base::Value(*value) : base::Value();
  };

  // Prefix with a ! so this appears at the top when serialized.
  root.Set("!install_url", install_url.spec());
  root.Set("add_to_applications_menu", add_to_applications_menu);
  root.Set("add_to_desktop", add_to_desktop);
  root.Set("add_to_management", add_to_management);
  root.Set("add_to_quick_launch_bar", add_to_quick_launch_bar);
  root.Set("add_to_search", add_to_search);
  root.Set("additional_search_terms",
           ConvertStringList(additional_search_terms));
  root.Set("app_info_factory", static_cast<bool>(app_info_factory));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  root.Set("disable_if_arc_supported", disable_if_arc_supported);
  root.Set("disable_if_tablet_form_factor", disable_if_tablet_form_factor);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  root.Set("disable_if_touchscreen_with_stylus_not_supported",
           disable_if_touchscreen_with_stylus_not_supported);
  root.Set("expected_app_id", ConvertOptional(expected_app_id));
  root.Set("handles_file_open_intents", handles_file_open_intents);
  root.Set("fallback_app_name", ConvertOptional(fallback_app_name));
  root.Set("force_reinstall", force_reinstall);
  root.Set("force_reinstall_for_milestone",
           ConvertOptional(force_reinstall_for_milestone));
  root.Set("gate_on_feature", ConvertOptional(gate_on_feature));
  root.Set("gate_on_feature_or_installed",
           ConvertOptional(gate_on_feature_or_installed));
  root.Set("install_placeholder", install_placeholder);
  root.Set("install_source", static_cast<int>(install_source));
  root.Set("is_disabled", is_disabled);
  root.Set("is_preferred_app_for_supported_links",
           is_preferred_app_for_supported_links);
  root.Set("launch_query_params", ConvertOptional(launch_query_params));
  root.Set("load_and_await_service_worker_registration",
           load_and_await_service_worker_registration);
  root.Set("oem_installed", oem_installed);
  root.Set("only_for_new_users", only_for_new_users);
  root.Set("only_if_previously_preinstalled", only_if_previously_preinstalled);
  root.Set("only_use_app_info_factory", only_use_app_info_factory);
  root.Set("override_previous_user_uninstall",
           override_previous_user_uninstall);
  root.Set("require_manifest", require_manifest);
  root.Set("install_as_shortcut", install_as_shortcut);
  root.Set("service_worker_registration_url",
           service_worker_registration_url
               ? base::Value(service_worker_registration_url->spec())
               : base::Value());
  root.Set("service_worker_registration_timeout_in_seconds",
           service_worker_registration_timeout.InSecondsF());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  root.Set("system_app_type",
           system_app_type ? base::Value(static_cast<int>(*system_app_type))
                           : base::Value());
#endif
  root.Set("uninstall_and_replace", ConvertStringList(uninstall_and_replace));
  root.Set("user_display_mode", user_display_mode.has_value()
                                    ? base::ToString(*user_display_mode)
                                    : "");
  root.Set("user_type_allowlist", ConvertStringList(user_type_allowlist));
  root.Set("placeholder_resolution_behavior",
           base::Value(static_cast<int>(placeholder_resolution_behavior)));
  root.Set("install_without_os_integration", install_without_os_integration);

  return base::Value(std::move(root));
}

WebAppInstallParams ConvertExternalInstallOptionsToParams(
    const ExternalInstallOptions& install_options) {
  WebAppInstallParams params;

  params.force_reinstall = install_options.force_reinstall;

  params.user_display_mode = install_options.user_display_mode;

  if (install_options.fallback_app_name.has_value()) {
    params.fallback_app_name =
        base::UTF8ToUTF16(install_options.fallback_app_name.value());
  }

  params.fallback_start_url = install_options.install_url;

  params.add_to_applications_menu = install_options.add_to_applications_menu;
  params.add_to_desktop = install_options.add_to_desktop;
  params.add_to_quick_launch_bar = install_options.add_to_quick_launch_bar;
  params.add_to_search = install_options.add_to_search;
  params.add_to_management = install_options.add_to_management;
  params.is_disabled = install_options.is_disabled;
  params.handles_file_open_intents = install_options.handles_file_open_intents;

  params.require_manifest = install_options.require_manifest;
  params.install_as_shortcut = install_options.install_as_shortcut;

  params.additional_search_terms = install_options.additional_search_terms;

  params.launch_query_params = install_options.launch_query_params;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  params.system_app_type = install_options.system_app_type;
#endif

  params.oem_installed = install_options.oem_installed;

  params.install_url = install_options.install_url;

  if (install_options.install_without_os_integration) {
    params.install_state =
        proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;
    params.add_to_search = false;
  }

  return params;
}

}  // namespace web_app
