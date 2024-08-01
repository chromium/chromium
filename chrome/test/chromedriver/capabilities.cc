// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/capabilities.h"

#include <map>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/client_hints.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/prompt_behavior.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"

namespace {

typedef base::RepeatingCallback<Status(const base::Value&, Capabilities*)>
    Parser;

Status ParseBoolean(
    bool* to_set,
    const base::Value& option,
    Capabilities* capabilities) {
  if (!option.is_bool())
    return Status(kInvalidArgument, "must be a boolean");
  if (to_set)
    *to_set = option.GetBool();
  return Status(kOk);
}

Status ParseString(std::string* to_set,
                   const base::Value& option,
                   Capabilities* capabilities) {
  const std::string* str = option.GetIfString();
  if (!str)
    return Status(kInvalidArgument, "must be a string");
  if (str->empty())
    return Status(kInvalidArgument, "cannot be empty");
  *to_set = *str;
  return Status(kOk);
}

Status ParseInterval(int* to_set,
                     const base::Value& option,
                     Capabilities* capabilities) {
  if (!option.is_int())
    return Status(kInvalidArgument, "must be an integer");
  if (option.GetInt() <= 0)
    return Status(kInvalidArgument, "must be positive");
  *to_set = option.GetInt();
  return Status(kOk);
}

Status ParseTimeDelta(base::TimeDelta* to_set,
                      const base::Value& option,
                      Capabilities* capabilities) {
  if (!option.is_int())
    return Status(kInvalidArgument, "must be an integer");
  if (option.GetInt() < 0)
    return Status(kInvalidArgument, "must be positive or zero");
  *to_set = base::Milliseconds(option.GetInt());
  return Status(kOk);
}

Status ParseFilePath(base::FilePath* to_set,
                     const base::Value& option,
                     Capabilities* capabilities) {
  if (!option.is_string())
    return Status(kInvalidArgument, "must be a string");
  *to_set = base::FilePath::FromUTF8Unsafe(option.GetString());
  return Status(kOk);
}

Status ParseDict(std::unique_ptr<base::Value::Dict>* to_set,
                 const base::Value& option,
                 Capabilities* capabilities) {
  const base::Value::Dict* dict = option.GetIfDict();
  if (!dict)
    return Status(kInvalidArgument, "must be a dictionary");
  *to_set = std::make_unique<base::Value::Dict>(dict->Clone());
  return Status(kOk);
}

Status IgnoreDeprecatedOption(
    const char* option_name,
    const base::Value& option,
    Capabilities* capabilities) {
  LOG(WARNING) << "Deprecated " << base::ToLowerASCII(kBrowserShortName)
               << " option is ignored: " << option_name;
  return Status(kOk);
}

Status IgnoreCapability(const base::Value& option, Capabilities* capabilities) {
  return Status(kOk);
}

Status ParseLogPath(const base::Value& option, Capabilities* capabilities) {
  if (!option.is_string())
    return Status(kInvalidArgument, "must be a string");
  capabilities->log_path = option.GetString();
  return Status(kOk);
}

Status ParseDeviceName(const std::string& device_name,
                       Capabilities* capabilities) {
  MobileDevice device;
  Status status = MobileDevice::FindMobileDevice(device_name, &device);

  if (status.IsError()) {
    return Status(kInvalidArgument,
                  "'" + device_name + "' must be a valid device", status);
  }

  capabilities->mobile_device = std::move(device);

  return Status(kOk);
}

Status ParseMobileEmulation(const base::Value& option,
                            Capabilities* capabilities) {
  const base::Value::Dict* mobile_emulation = option.GetIfDict();
  if (!mobile_emulation)
    return Status(kInvalidArgument, "'mobileEmulation' must be a dictionary");

  if (mobile_emulation->Find("deviceName")) {
    // Cannot use any other options with deviceName.
    if (mobile_emulation->size() > 1)
      return Status(kInvalidArgument, "'deviceName' must be used alone");

    const std::string* device_name = mobile_emulation->FindString("deviceName");
    if (!device_name)
      return Status(kInvalidArgument, "'deviceName' must be a string");

    return ParseDeviceName(*device_name, capabilities);
  }

  MobileDevice mobile_device;

  bool mobile_ua = false;

  if (mobile_emulation->Find("userAgent")) {
    const std::string* user_agent = mobile_emulation->FindString("userAgent");
    if (!user_agent) {
      return Status(kInvalidArgument, "'userAgent' must be a string");
    }
    mobile_device.user_agent = *user_agent;

    mobile_ua =
        std::string_view{*user_agent}.find("Mobile") != std::string_view::npos;
  }

  if (mobile_emulation->Find("deviceMetrics")) {
    const base::Value::Dict* metrics =
        mobile_emulation->FindDict("deviceMetrics");
    if (!metrics)
      return Status(kInvalidArgument, "'deviceMetrics' must be a dictionary");

    const base::Value* width_value = metrics->Find("width");
    if (width_value && !width_value->is_int())
      return Status(kInvalidArgument, "'width' must be an integer");

    int width = width_value ? width_value->GetInt() : 0;

    const base::Value* height_value = metrics->Find("height");
    if (height_value && !height_value->is_int())
      return Status(kInvalidArgument, "'height' must be an integer");

    int height = height_value ? height_value->GetInt() : 0;

    std::optional<double> maybe_device_scale_factor =
        metrics->FindDouble("pixelRatio");
    if (metrics->Find("pixelRatio") && !maybe_device_scale_factor.has_value())
      return Status(kInvalidArgument, "'pixelRatio' must be a double");

    std::optional<bool> touch = metrics->FindBool("touch");
    if (metrics->Find("touch") && !touch.has_value())
      return Status(kInvalidArgument, "'touch' must be a boolean");

    std::optional<bool> mobile = metrics->FindBool("mobile");
    if (metrics->Find("mobile") && !mobile.has_value())
      return Status(kInvalidArgument, "'mobile' must be a boolean");
    if (!mobile.has_value()) {
      // Due to legacy reasons missing 'deviceMetrics.mobile' is inferred as
      // true.
      VLOG(logging::LOGGING_INFO) << "Inferring 'deviceMetrics.mobile' as true";
      mobile = true;
    }

    if (mobile_device.user_agent && mobile_ua && !mobile.value()) {
      // Presence of word Mobile in UserAgent clearly hints that the device is
      // mobile. The opposite is not true.
      VLOG(logging::LOGGING_WARNING)
          << "The mobility in 'userAgent' contradicts "
             "'deviceMetrics.mobile' value.";
    }

    if (!touch.has_value()) {
      VLOG(logging::LOGGING_INFO) << "Inferring 'deviceMetrics.touch' as true.";
    }
    if (!maybe_device_scale_factor.has_value()) {
      VLOG(logging::LOGGING_INFO)
          << "Inferring 'deviceMetrics.pixelRatio' as 0.";
    }

    DeviceMetrics device_metrics{width, height,
                                 maybe_device_scale_factor.value_or(0),
                                 touch.value_or(true), mobile.value()};
    mobile_device.device_metrics = std::move(device_metrics);
  }

  if (mobile_ua && !mobile_device.device_metrics.has_value()) {
    VLOG(logging::LOGGING_INFO)
        << "The 'userAgent' value corresponds to a mobile UserAgent but "
           "'deviceMetrics' is not provided.";
  }

  if (mobile_emulation->Find("clientHints")) {
    if (!mobile_emulation->Find("clientHints")->is_dict()) {
      return Status{kInvalidArgument, "'clientHints' must be a dictionary"};
    }
    const base::Value::Dict& client_hints_dict =
        *mobile_emulation->FindDict("clientHints");

    ClientHints client_hints;

    if (!client_hints_dict.Find("platform")) {
      return Status(kInvalidArgument,
                    "'clientHints.platform' must be provided");
    }
    const std::string* maybe_platform =
        client_hints_dict.FindString("platform");
    if (!maybe_platform) {
      return Status(kInvalidArgument,
                    "'clientHints.platform' must be a string");
    }
    client_hints.platform = *maybe_platform;
    std::vector<std::string> supported_platforms =
        MobileDevice::GetReducedUserAgentPlatforms();
    if (!mobile_device.user_agent.has_value() &&
        !base::Contains(supported_platforms, client_hints.platform)) {
      std::string supported_platforms_str =
          base::JoinString(supported_platforms, ", ");
      return Status(kInvalidArgument,
                    "'userAgent' is required for platforms other than: " +
                        supported_platforms_str);
    }

    std::optional<bool> mobile = client_hints_dict.FindBool("mobile");
    if (client_hints_dict.Find("mobile") && !mobile.has_value()) {
      return Status(kInvalidArgument, "'clientHints.mobile' must be a boolean");
    }
    if (!mobile.has_value()) {
      if (base::ToUpperASCII(client_hints.platform) == "ANDROID" &&
          mobile_device.user_agent.has_value()) {
        VLOG(logging::LOGGING_INFO)
            << "Inferring 'clientHints.mobile' from 'userAgent' as "
            << mobile_ua;
        mobile = mobile_ua;
      } else {
        VLOG(logging::LOGGING_INFO)
            << "Inferring 'clientHints.mobile' as false";
        mobile = false;
      }
    }
    if (mobile_device.device_metrics.has_value()) {
      if (mobile.has_value() && mobile.value() &&
          !mobile_device.device_metrics->mobile) {
        VLOG(logging::LOGGING_WARNING)
            << "The mobility in 'clientHints.mobile' contradicts "
               "'deviceMetrics.mobile' value.";
      }
    }
    // All the paths above assign some value to 'mobile'
    client_hints.mobile = mobile.value();

    if (client_hints_dict.Find("architecture")) {
      const std::string* architecture =
          client_hints_dict.FindString("architecture");
      if (!architecture) {
        return Status(kInvalidArgument,
                      "'clientHints.architecture' must be a string");
      }
      client_hints.architecture = *architecture;
    } else {
      VLOG(logging::LOGGING_INFO)
          << "Inferring 'clientHints.architecture' as an empty string.";
      client_hints.architecture = "";
    }

    if (client_hints_dict.Find("bitness")) {
      const std::string* bitness = client_hints_dict.FindString("bitness");
      if (!bitness) {
        return Status(kInvalidArgument,
                      "'clientHints.bitness' must be a string");
      }
      client_hints.bitness = *bitness;
    } else {
      VLOG(logging::LOGGING_INFO)
          << "Inferring 'clientHints.bitness' as an empty string.";
      client_hints.bitness = "";
    }

    if (client_hints_dict.Find("brands")) {
      const base::Value::List* brand_list =
          client_hints_dict.FindList("brands");
      if (!brand_list) {
        return Status(kInvalidArgument,
                      "'clientHints.brands' must be an array of objects");
      }

      std::vector<BrandVersion> brands;
      for (const base::Value& item : *brand_list) {
        if (!item.is_dict()) {
          return Status(kInvalidArgument,
                        "each 'clientHints.brands' entry must be an object");
        }
        const std::string* brand = item.GetDict().FindString("brand");
        if (!brand) {
          return Status(kInvalidArgument,
                        "each 'clientHints.brands' entry must have a 'brand' "
                        "field of type string");
        }
        const std::string* version = item.GetDict().FindString("version");
        if (!version) {
          return Status(kInvalidArgument,
                        "each 'clientHints.brands' entry must have a "
                        "'version' field of type string");
        }

        brands.emplace_back(*brand, *version);
      }

      client_hints.brands = std::move(brands);
    } else {
      VLOG(logging::LOGGING_INFO)
          << "Inferring 'clientHints.brands' as browser defined.";
    }

    if (client_hints_dict.Find("fullVersionList")) {
      const base::Value::List* full_version_list_list =
          client_hints_dict.FindList("fullVersionList");
      if (!full_version_list_list) {
        return Status(
            kInvalidArgument,
            "'clientHints.fullVersionList' must be an array of objects");
      }

      std::vector<BrandVersion> full_version_list;
      for (const base::Value& item : *full_version_list_list) {
        if (!item.is_dict()) {
          return Status(
              kInvalidArgument,
              "each 'clientHints.fullVersionList' entry must be an object");
        }
        const std::string* brand = item.GetDict().FindString("brand");
        if (!brand) {
          return Status(kInvalidArgument,
                        "each 'clientHints.fullVersionList' entry must have "
                        "a 'brand' field of type string");
        }
        const std::string* version = item.GetDict().FindString("version");
        if (!version) {
          return Status(kInvalidArgument,
                        "each 'clientHints.fullVersionList' entry must have "
                        "a 'version' field of type string");
        }

        full_version_list.emplace_back(*brand, *version);
      }

      client_hints.full_version_list = std::move(full_version_list);
    } else {
      VLOG(logging::LOGGING_INFO)
          << "Inferring 'clientHints.fullversionList' as browser defined.";
    }

    if (client_hints_dict.Find("model")) {
      const std::string* model = client_hints_dict.FindString("model");
      if (!model) {
        return Status(kInvalidArgument, "'clientHints.model' must be a string");
      }
      if (!client_hints.mobile && model->size() > 0) {
        VLOG(logging::LOGGING_INFO)
            << "User provides 'clientHints.model' for a non-mobile "
               "platform as indicated by 'clientHints.mobile'";
      }
      client_hints.model = *model;
    } else {
      VLOG(logging::LOGGING_INFO)
          << "Inferring 'clientHints.model' as an empty string.";
      client_hints.model = "";
    }

    if (client_hints_dict.Find("platformVersion")) {
      const std::string* platform_version =
          client_hints_dict.FindString("platformVersion");
      if (!platform_version) {
        return Status(kInvalidArgument,
                      "'clientHints.platformVersion' must be a string");
      }
      client_hints.platform_version = *platform_version;
    } else {
      VLOG(logging::LOGGING_INFO)
          << "Inferring 'clientHints.platformVersion' as an empty string.";
      client_hints.platform_version = "";
    }

    if (client_hints_dict.Find("wow64")) {
      std::optional<bool> wow64 = client_hints_dict.FindBool("wow64");
      if (!wow64.has_value()) {
        return Status(kInvalidArgument,
                      "'clientHints.wow64' must be a boolean");
      }
      client_hints.wow64 = *wow64;
    } else {
      VLOG(logging::LOGGING_INFO) << "Inferring 'clientHints.wow64' as false.";
      client_hints.wow64 = false;
    }

    mobile_device.client_hints = std::move(client_hints);
  } else if (mobile_device.user_agent.has_value()) {
    VLOG(logging::LOGGING_INFO)
        << "Operating in legacy emulation mode as 'mobileEmulation' contains "
           "no 'clientHints'.";
    ClientHints client_hints;
    if (!MobileDevice::GuessPlatform(mobile_device.user_agent.value(),
                                     &client_hints.platform)) {
      // In legacy mode we allow platform to be empty.
      // Otherwise we might break the users' tests.
      client_hints.platform = "";
    }
    client_hints.mobile =
        client_hints.platform == "Android" ? mobile_ua : false;
    // Empty value corresponds to the result of GetCpuArchitecture in
    // //content/common/user_agent.cc.
    client_hints.architecture = "";
    // Empty value corresponds to the result of GetCpuBitness in
    // //content/common/user_agent.cc.
    client_hints.bitness = "";
    client_hints.model = "";
    client_hints.platform_version = "";
    client_hints.wow64 = false;
    VLOG(logging::LOGGING_INFO)
        << "No 'clientHints' found. Operating in legacy mode. "
        << "Inferring clientHints as: "
        << "{architecture='" << client_hints.architecture << "'"
        << ", bitness='" << client_hints.bitness << "'"
        << ", brands=<browser-defined>"
        << ", fullVersionList=<browser-defined>"
        << ", mobile=" << std::boolalpha << client_hints.mobile << ", model='"
        << client_hints.model << "'"
        << ", platform='" << client_hints.platform << "'"
        << ", platformVersion='" << client_hints.platform_version << "'"
        << ", wow64=" << std::boolalpha << client_hints.wow64 << "}";
    mobile_device.client_hints = std::move(client_hints);
  }

  capabilities->mobile_device = std::move(mobile_device);

  return Status(kOk);
}

Status ParsePageLoadStrategy(const base::Value& option,
                             Capabilities* capabilities) {
  if (!option.is_string())
    return Status(kInvalidArgument, "'pageLoadStrategy' must be a string");
  capabilities->page_load_strategy = option.GetString();
  if (capabilities->page_load_strategy == PageLoadStrategy::kNone ||
      capabilities->page_load_strategy == PageLoadStrategy::kEager ||
      capabilities->page_load_strategy == PageLoadStrategy::kNormal)
    return Status(kOk);
  return Status(kInvalidArgument, "invalid 'pageLoadStrategy'");
}

Status ParseUnhandledPromptBehavior(bool w3c_compliant,
                                    const base::Value& option,
                                    Capabilities* capabilities) {
  PromptBehavior unhandled_prompt_behavior(w3c_compliant);
  Status status =
      PromptBehavior::Create(w3c_compliant, option, unhandled_prompt_behavior);
  if (status.IsError()) {
    return status;
  }
  capabilities->unhandled_prompt_behavior =
      std::move(unhandled_prompt_behavior);
  return Status(kOk);
}

Status ParseTimeouts(const base::Value& option, Capabilities* capabilities) {
  const base::Value::Dict* timeouts = option.GetIfDict();
  if (!timeouts)
    return Status(kInvalidArgument, "'timeouts' must be a JSON object");
  for (auto it : *timeouts) {
    int64_t timeout_ms_int64 = -1;
    base::TimeDelta timeout;
    const std::string& type = it.first;
    if (it.second.is_none()) {
      if (type == "script")
        timeout = base::TimeDelta::Max();
      else
        return Status(kInvalidArgument, "timeout can not be null");
    } else {
      if (!GetOptionalSafeInt(*timeouts, it.first, &timeout_ms_int64) ||
          timeout_ms_int64 < 0)
        return Status(kInvalidArgument, "value must be a non-negative integer");
      else
        timeout = base::Milliseconds(timeout_ms_int64);
    }
    if (type == "script") {
      capabilities->script_timeout = timeout;
    } else if (type == "pageLoad") {
      capabilities->page_load_timeout = timeout;
    } else if (type == "implicit") {
      capabilities->implicit_wait_timeout = timeout;
    } else {
      return Status(kInvalidArgument,
                    "unrecognized 'timeouts' option: " + type);
    }
  }
  return Status(kOk);
}

Status ParseSwitches(const base::Value& option,
                     Capabilities* capabilities) {
  if (!option.is_list())
    return Status(kInvalidArgument, "must be a list");
  for (const base::Value& arg : option.GetList()) {
    if (!arg.is_string())
      return Status(kInvalidArgument, "each argument must be a string");
    std::string arg_string = arg.GetString();
    base::TrimWhitespaceASCII(arg_string, base::TRIM_ALL, &arg_string);
    if (arg_string.empty() || arg_string == "--")
      return Status(kInvalidArgument, "argument is empty");
    capabilities->switches.SetUnparsedSwitch(std::move(arg_string));
  }
  return Status(kOk);
}

Status ParseExtensions(const base::Value& option, Capabilities* capabilities) {
  if (!option.is_list())
    return Status(kInvalidArgument, "must be a list");
  for (const base::Value& extension : option.GetList()) {
    if (!extension.is_string()) {
      return Status(kInvalidArgument,
                    "each extension must be a base64 encoded string");
    }
    capabilities->extensions.push_back(extension.GetString());
  }
  return Status(kOk);
}

Status ParseProxy(bool w3c_compliant,
                  const base::Value& option,
                  Capabilities* capabilities) {
  const base::Value::Dict* proxy_dict = option.GetIfDict();
  if (!proxy_dict)
    return Status(kInvalidArgument, "must be a dictionary");
  const std::string* proxy_type_str = proxy_dict->FindString("proxyType");
  if (!proxy_type_str)
    return Status(kInvalidArgument, "'proxyType' must be a string");
  std::string proxy_type =
      w3c_compliant ? *proxy_type_str : base::ToLowerASCII(*proxy_type_str);
  if (proxy_type == "direct") {
    capabilities->switches.SetSwitch("no-proxy-server");
  } else if (proxy_type == "system") {
    // Chrome default.
  } else if (proxy_type == "pac") {
    const std::string* proxy_pac_url =
        proxy_dict->FindString("proxyAutoconfigUrl");
    if (!proxy_pac_url)
      return Status(kInvalidArgument, "'proxyAutoconfigUrl' must be a string");
    capabilities->switches.SetSwitch("proxy-pac-url", *proxy_pac_url);
  } else if (proxy_type == "autodetect") {
    capabilities->switches.SetSwitch("proxy-auto-detect");
  } else if (proxy_type == "manual") {
    const char* const proxy_servers_options[][2] = {
        {"ftpProxy", "ftp"}, {"httpProxy", "http"}, {"sslProxy", "https"},
        {"socksProxy", "socks"}};
    const std::string kSocksProxy = "socksProxy";
    const base::Value* option_value = nullptr;
    std::string proxy_servers;
    for (const char* const* proxy_servers_option : proxy_servers_options) {
      option_value = proxy_dict->Find(proxy_servers_option[0]);
      if (option_value == nullptr || option_value->is_none()) {
        continue;
      }
      if (!option_value->is_string()) {
        return Status(kInvalidArgument,
                      base::StringPrintf("'%s' must be a string",
                                         proxy_servers_option[0]));
      }
      std::string value = option_value->GetString();
      if (proxy_servers_option[0] == kSocksProxy) {
        int socks_version = proxy_dict->FindInt("socksVersion").value_or(-1);
        if (socks_version < 0 || socks_version > 255)
          return Status(
              kInvalidArgument,
              "'socksVersion' must be between 0 and 255");
        value =
            base::StringPrintf("socks%d://%s", socks_version, value.c_str());
      }
      // Converts into Chrome proxy scheme.
      // Example: "http=localhost:9000;ftp=localhost:8000".
      if (!proxy_servers.empty())
        proxy_servers += ";";
      proxy_servers +=
          base::StringPrintf("%s=%s", proxy_servers_option[1], value.c_str());
    }

    std::string proxy_bypass_list;
    option_value = proxy_dict->Find("noProxy");
    if (option_value != nullptr && !option_value->is_none()) {
      // W3C requires noProxy to be a list of strings, while legacy protocol
      // requires noProxy to be a string of comma-separated items.
      // In practice, library implementations are not always consistent,
      // so we accept both formats regardless of the W3C mode setting.
      if (option_value->is_list()) {
        for (const base::Value& item : option_value->GetList()) {
          if (!item.is_string())
            return Status(kInvalidArgument,
                          "'noProxy' must be a list of strings");
          if (!proxy_bypass_list.empty())
            proxy_bypass_list += ",";
          proxy_bypass_list += item.GetString();
        }
      } else if (option_value->is_string()) {
        proxy_bypass_list = option_value->GetString();
      } else {
        return Status(kInvalidArgument, "'noProxy' must be a list or a string");
      }
    }

    // W3C doesn't require specifying any proxy servers even when proxyType is
    // manual, even though such a setting would be useless.
    if (!proxy_servers.empty())
      capabilities->switches.SetSwitch("proxy-server", proxy_servers);
    if (!proxy_bypass_list.empty()) {
      capabilities->switches.SetSwitch("proxy-bypass-list",
                                       proxy_bypass_list);
    }
  } else {
    return Status(kInvalidArgument, "unrecognized proxy type: " + proxy_type);
  }
  return Status(kOk);
}

Status ParseExcludeSwitches(const base::Value& option,
                            Capabilities* capabilities) {
  if (!option.is_list())
    return Status(kInvalidArgument, "must be a list");
  for (const base::Value& switch_value : option.GetList()) {
    if (!switch_value.is_string()) {
      return Status(kInvalidArgument,
                    "each switch to be removed must be a string");
    }
    std::string switch_name = switch_value.GetString();
    if (switch_name.substr(0, 2) == "--")
      switch_name = switch_name.substr(2);
    capabilities->exclude_switches.insert(std::move(switch_name));
  }
  return Status(kOk);
}

Status ParsePortNumber(int* to_set,
                     const base::Value& option,
                     Capabilities* capabilities) {
  int max_port_number = 65535;
  if (!option.is_int())
    return Status(kInvalidArgument, "must be an integer");
  if (option.GetInt() <= 0)
    return Status(kInvalidArgument, "must be positive");
  if (option.GetInt() > max_port_number)
    return Status(kInvalidArgument, "must be less than or equal to " +
                                    base::NumberToString(max_port_number));
  *to_set = option.GetInt();
  return Status(kOk);
}


Status ParseNetAddress(NetAddress* to_set,
                       const base::Value& option,
                       Capabilities* capabilities) {
  if (!option.is_string())
    return Status(kInvalidArgument, "must be 'host:port'");
  std::string server_addr = option.GetString();
  std::vector<std::string> values;
  if (base::StartsWith(server_addr, "[")) {
    size_t ipv6_terminator_pos = server_addr.find(']');
    if (ipv6_terminator_pos == std::string::npos) {
      return Status(kInvalidArgument,
                    "ipv6 address must be terminated with ']'");
    }
    values.push_back(server_addr.substr(0, ipv6_terminator_pos + 1));
    std::vector<std::string> remaining =
        base::SplitString(server_addr.substr(ipv6_terminator_pos + 1), ":",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    values.insert(values.end(), remaining.begin(), remaining.end());
  } else {
    values = base::SplitString(server_addr, ":", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_ALL);
  }

  if (values.size() != 2)
    return Status(kInvalidArgument, "must be 'host:port'");

  int port = 0;
  base::StringToInt(values[1], &port);
  if (port <= 0)
    return Status(kInvalidArgument, "port must be > 0");

  *to_set = NetAddress(values[0], port);
  return Status(kOk);
}

Status ParseLoggingPrefs(const base::Value& option,
                         Capabilities* capabilities) {
  const base::Value::Dict* logging_prefs = option.GetIfDict();
  if (!logging_prefs)
    return Status(kInvalidArgument, "must be a dictionary");

  for (const auto pref : *logging_prefs) {
    const std::string& type = pref.first;
    Log::Level level;
    const std::string* level_name = pref.second.GetIfString();
    if (!level_name || !WebDriverLog::NameToLevel(*level_name, &level)) {
      return Status(kInvalidArgument,
                    "invalid log level for '" + type + "' log");
    }
    capabilities->logging_prefs.insert(std::make_pair(type, level));
  }
  return Status(kOk);
}

Status ParseInspectorDomainStatus(
    PerfLoggingPrefs::InspectorDomainStatus* to_set,
    const base::Value& option,
    Capabilities* capabilities) {
  if (!option.is_bool())
    return Status(kInvalidArgument, "must be a boolean");
  if (option.GetBool())
    *to_set = PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled;
  else
    *to_set = PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyDisabled;
  return Status(kOk);
}

Status ParsePerfLoggingPrefs(const base::Value& option,
                             Capabilities* capabilities) {
  const base::Value::Dict* perf_logging_prefs = option.GetIfDict();
  if (!perf_logging_prefs)
    return Status(kInvalidArgument, "must be a dictionary");

  std::map<std::string, Parser> parser_map;
  parser_map["bufferUsageReportingInterval"] = base::BindRepeating(
      &ParseInterval,
      &capabilities->perf_logging_prefs.buffer_usage_reporting_interval);
  parser_map["enableNetwork"] = base::BindRepeating(
      &ParseInspectorDomainStatus, &capabilities->perf_logging_prefs.network);
  parser_map["enablePage"] = base::BindRepeating(
      &ParseInspectorDomainStatus, &capabilities->perf_logging_prefs.page);
  parser_map["traceCategories"] = base::BindRepeating(
      &ParseString, &capabilities->perf_logging_prefs.trace_categories);

  for (const auto item : *perf_logging_prefs) {
    if (parser_map.find(item.first) == parser_map.end())
      return Status(kInvalidArgument,
                    "unrecognized performance logging option: " + item.first);
    Status status = parser_map[item.first].Run(item.second, capabilities);
    if (status.IsError())
      return Status(kInvalidArgument, "cannot parse " + item.first, status);
  }
  return Status(kOk);
}

Status ParseDevToolsEventsLoggingPrefs(const base::Value& option,
                                       Capabilities* capabilities) {
  if (!option.is_list())
    return Status(kInvalidArgument, "must be a list");
  if (option.GetList().empty())
    return Status(kInvalidArgument, "list must contain values");
  capabilities->devtools_events_logging_prefs = option.Clone();
  return Status(kOk);
}

Status ParseWindowTypes(const base::Value& option, Capabilities* capabilities) {
  if (!option.is_list())
    return Status(kInvalidArgument, "must be a list");
  std::set<WebViewInfo::Type> window_types_tmp;
  for (const base::Value& window_type : option.GetList()) {
    if (!window_type.is_string()) {
      return Status(kInvalidArgument, "each window type must be a string");
    }
    WebViewInfo::Type type;
    Status status = WebViewInfo::ParseType(window_type.GetString(), type);
    if (status.IsError())
      return status;
    window_types_tmp.insert(type);
  }
  capabilities->window_types.swap(window_types_tmp);
  return Status(kOk);
}

Status ParseChromeOptions(
    const base::Value& capability,
    Capabilities* capabilities) {
  const base::Value::Dict* chrome_options = capability.GetIfDict();
  if (!chrome_options)
    return Status(kInvalidArgument, "must be a dictionary");

  bool is_android = chrome_options->Find("androidPackage") != nullptr;
  bool is_remote = chrome_options->Find("debuggerAddress") != nullptr;

  std::map<std::string, Parser> parser_map;
  // Ignore 'args', 'binary' and 'extensions' capabilities by default, since the
  // Java client always passes them.
  parser_map["args"] = base::BindRepeating(&IgnoreCapability);
  parser_map["binary"] = base::BindRepeating(&IgnoreCapability);
  parser_map["extensions"] = base::BindRepeating(&IgnoreCapability);

  parser_map["perfLoggingPrefs"] = base::BindRepeating(&ParsePerfLoggingPrefs);
  parser_map["devToolsEventsToLog"] =
      base::BindRepeating(&ParseDevToolsEventsLoggingPrefs);
  parser_map["windowTypes"] = base::BindRepeating(&ParseWindowTypes);
  // Compliance is read when session is initialized and correct response is
  // sent if not parsed correctly.
  parser_map["w3c"] = base::BindRepeating(&IgnoreCapability);

  if (is_android) {
    parser_map["androidActivity"] =
        base::BindRepeating(&ParseString, &capabilities->android_activity);
    parser_map["androidDeviceSerial"] =
        base::BindRepeating(&ParseString, &capabilities->android_device_serial);
    parser_map["androidPackage"] =
        base::BindRepeating(&ParseString, &capabilities->android_package);
    parser_map["androidProcess"] =
        base::BindRepeating(&ParseString, &capabilities->android_process);
    parser_map["androidExecName"] =
        base::BindRepeating(&ParseString, &capabilities->android_exec_name);
    parser_map["androidDeviceSocket"] =
        base::BindRepeating(&ParseString, &capabilities->android_device_socket);
    parser_map["androidUseRunningApp"] = base::BindRepeating(
        &ParseBoolean, &capabilities->android_use_running_app);
    parser_map["androidKeepAppDataDir"] = base::BindRepeating(
        &ParseBoolean, &capabilities->android_keep_app_data_dir);
    parser_map["androidDevToolsPort"] = base::BindRepeating(
        &ParsePortNumber, &capabilities->android_devtools_port);
    parser_map["args"] = base::BindRepeating(&ParseSwitches);
    parser_map["excludeSwitches"] = base::BindRepeating(&ParseExcludeSwitches);
    parser_map["loadAsync"] =
        base::BindRepeating(&IgnoreDeprecatedOption, "loadAsync");
  } else if (is_remote) {
    parser_map["debuggerAddress"] =
        base::BindRepeating(&ParseNetAddress, &capabilities->debugger_address);
  } else {
    parser_map["args"] = base::BindRepeating(&ParseSwitches);
    parser_map["binary"] =
        base::BindRepeating(&ParseFilePath, &capabilities->binary);
    parser_map["detach"] =
        base::BindRepeating(&ParseBoolean, &capabilities->detach);
    parser_map["excludeSwitches"] = base::BindRepeating(&ParseExcludeSwitches);
    parser_map["extensions"] = base::BindRepeating(&ParseExtensions);
    parser_map["extensionLoadTimeout"] = base::BindRepeating(
        &ParseTimeDelta, &capabilities->extension_load_timeout);
    parser_map["loadAsync"] =
        base::BindRepeating(&IgnoreDeprecatedOption, "loadAsync");
    parser_map["localState"] =
        base::BindRepeating(&ParseDict, &capabilities->local_state);
    parser_map["logPath"] = base::BindRepeating(&ParseLogPath);
    parser_map["minidumpPath"] =
        base::BindRepeating(&ParseString, &capabilities->minidump_path);
    parser_map["mobileEmulation"] = base::BindRepeating(&ParseMobileEmulation);
    parser_map["prefs"] = base::BindRepeating(&ParseDict, &capabilities->prefs);
    parser_map["useAutomationExtension"] =
        base::BindRepeating(&IgnoreDeprecatedOption, "useAutomationExtension");
    parser_map["browserStartupTimeout"] = base::BindRepeating(
        &ParseTimeDelta, &capabilities->browser_startup_timeout);
  }

  for (const auto item : *chrome_options) {
    if (parser_map.find(item.first) == parser_map.end()) {
      return Status(
          kInvalidArgument,
          base::StringPrintf("unrecognized %s option: %s",
                             base::ToLowerASCII(kBrowserShortName).c_str(),
                             item.first.c_str()));
    }
    Status status = parser_map[item.first].Run(item.second, capabilities);
    if (status.IsError())
      return Status(kInvalidArgument, "cannot parse " + item.first, status);
  }
  return Status(kOk);
}

Status ParseSeleniumOptions(
    const base::Value& capability,
    Capabilities* capabilities) {
  const base::Value::Dict* selenium_options = capability.GetIfDict();
  if (!selenium_options)
    return Status(kInvalidArgument, "must be a dictionary");
  std::map<std::string, Parser> parser_map;
  parser_map["loggingPrefs"] = base::BindRepeating(&ParseLoggingPrefs);

  for (const auto item : *selenium_options) {
    if (parser_map.find(item.first) == parser_map.end())
      continue;
    Status status = parser_map[item.first].Run(item.second, capabilities);
    if (status.IsError())
      return Status(kInvalidArgument, "cannot parse " + item.first, status);
  }
  return Status(kOk);
}
}  // namespace

bool GetChromeOptionsDictionary(const base::Value::Dict& params,
                                const base::Value::Dict** out) {
  const base::Value::Dict* result =
      params.FindDict(kChromeDriverOptionsKeyPrefixed);
  if (result) {
    *out = result;
    return true;
  }
  result = params.FindDict(kChromeDriverOptionsKey);
  if (result) {
    *out = result;
    return true;
  }
  return false;
}

Switches::Switches() = default;

Switches::Switches(const Switches& other) = default;

Switches::~Switches() = default;

void Switches::SetSwitch(const std::string& name) {
  SetSwitch(name, std::string());
}

void Switches::SetSwitch(const std::string& name, const std::string& value) {
#if BUILDFLAG(IS_WIN)
  switch_map_[name] = base::UTF8ToWide(value);
#else
  switch_map_[name] = value;
#endif
}

void Switches::SetSwitch(const std::string& name, const base::FilePath& value) {
  switch_map_[name] = value.value();
}

void Switches::SetMultivaluedSwitch(const std::string& name,
                                    const std::string& value) {
#if BUILDFLAG(IS_WIN)
  auto native_value = base::UTF8ToWide(value);
  auto delimiter = L',';
#else
  const auto& native_value = value;
  const auto delimiter = ',';
#endif
  NativeString& switch_value = switch_map_[name];
  if (switch_value.size() > 0 && switch_value.back() != delimiter) {
    switch_value += delimiter;
  }
  switch_value += native_value;
}

void Switches::SetFromSwitches(const Switches& switches) {
  for (auto iter = switches.switch_map_.begin();
       iter != switches.switch_map_.end(); ++iter) {
    switch_map_[iter->first] = iter->second;
  }
}

namespace {
constexpr auto kMultivaluedSwitches = base::MakeFixedFlatSet<std::string_view>(
    {"enable-blink-features", "disable-blink-features", "enable-features",
     "disable-features"});
}  // namespace

void Switches::SetUnparsedSwitch(const std::string& unparsed_switch) {
  std::string value;
  size_t equals_index = unparsed_switch.find('=');
  if (equals_index != std::string::npos)
    value = unparsed_switch.substr(equals_index + 1);

  std::string name;
  size_t start_index = 0;
  if (unparsed_switch.substr(0, 2) == "--")
    start_index = 2;
  name = unparsed_switch.substr(start_index, equals_index - start_index);

  if (kMultivaluedSwitches.contains(name))
    SetMultivaluedSwitch(name, value);
  else
    SetSwitch(name, value);
}

void Switches::RemoveSwitch(const std::string& name) {
  switch_map_.erase(name);
}

bool Switches::HasSwitch(const std::string& name) const {
  return switch_map_.count(name) > 0;
}

std::string Switches::GetSwitchValue(const std::string& name) const {
  NativeString value = GetSwitchValueNative(name);
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(value);
#else
  return value;
#endif
}

Switches::NativeString Switches::GetSwitchValueNative(
    const std::string& name) const {
  auto iter = switch_map_.find(name);
  if (iter == switch_map_.end())
    return NativeString();
  return iter->second;
}

size_t Switches::GetSize() const {
  return switch_map_.size();
}

void Switches::AppendToCommandLine(base::CommandLine* command) const {
  for (auto iter = switch_map_.begin(); iter != switch_map_.end(); ++iter) {
    command->AppendSwitchNative(iter->first, iter->second);
  }
}

std::string Switches::ToString() const {
  std::string str;
  auto iter = switch_map_.begin();
  while (iter != switch_map_.end()) {
    str += "--" + iter->first;
    std::string value = GetSwitchValue(iter->first);
    if (value.length()) {
      if (value.find(' ') != std::string::npos)
        value = base::GetQuotedJSONString(value);
      str += "=" + value;
    }
    ++iter;
    if (iter == switch_map_.end())
      break;
    str += " ";
  }
  return str;
}

PerfLoggingPrefs::PerfLoggingPrefs()
    : network(InspectorDomainStatus::kDefaultEnabled),
      page(InspectorDomainStatus::kDefaultEnabled),
      buffer_usage_reporting_interval(1000) {}

PerfLoggingPrefs::~PerfLoggingPrefs() = default;

Capabilities::Capabilities()
    : accept_insecure_certs(false),
      page_load_strategy(PageLoadStrategy::kNormal),
      strict_file_interactability(false),
      android_use_running_app(false),
      detach(false),
      extension_load_timeout(base::Seconds(10)),
      network_emulation_enabled(false) {}

Capabilities::~Capabilities() = default;

bool Capabilities::IsAndroid() const {
  return !android_package.empty();
}

bool Capabilities::IsRemoteBrowser() const {
  return debugger_address.IsValid();
}

Status Capabilities::Parse(const base::Value::Dict& desired_caps,
                           bool w3c_compliant) {
  std::map<std::string, Parser> parser_map;

  // W3C defined capabilities.
  parser_map["acceptInsecureCerts"] =
      base::BindRepeating(&ParseBoolean, &accept_insecure_certs);
  parser_map["browserName"] = base::BindRepeating(&ParseString, &browser_name);
  parser_map["browserVersion"] =
      base::BindRepeating(&ParseString, &browser_version);
  parser_map["platformName"] =
      base::BindRepeating(&ParseString, &platform_name);
  parser_map["pageLoadStrategy"] = base::BindRepeating(&ParsePageLoadStrategy);
  parser_map["proxy"] = base::BindRepeating(&ParseProxy, w3c_compliant);
  parser_map["timeouts"] = base::BindRepeating(&ParseTimeouts);
  parser_map["strictFileInteractability"] =
      base::BindRepeating(&ParseBoolean, &strict_file_interactability);
  parser_map["webSocketUrl"] =
      base::BindRepeating(&ParseBoolean, &web_socket_url);
  if (!w3c_compliant) {
    // TODO(https://crbug.com/chromedriver/2596): "unexpectedAlertBehaviour" is
    // legacy name of "unhandledPromptBehavior", remove when we stop supporting
    // legacy mode.
    parser_map["unexpectedAlertBehaviour"] =
        base::BindRepeating(&ParseUnhandledPromptBehavior, w3c_compliant);
  }
  parser_map["unhandledPromptBehavior"] =
      base::BindRepeating(&ParseUnhandledPromptBehavior, w3c_compliant);

  // W3C defined extension capabilities.
  // See https://w3c.github.io/webauthn/#sctn-automation-webdriver-capability
  parser_map["webauthn:virtualAuthenticators"] =
      base::BindRepeating(&ParseBoolean, nullptr);
  parser_map["webauthn:extension:largeBlob"] =
      base::BindRepeating(&ParseBoolean, nullptr);
  // See https://github.com/fedidcg/FedCM/pull/478
  parser_map["fedcm:accounts"] = base::BindRepeating(&ParseBoolean, nullptr);

  // ChromeDriver specific capabilities.
  // Vendor-prefixed is the current spec conformance, but unprefixed is
  // still supported in legacy mode.
  if (w3c_compliant || desired_caps.FindDict(kChromeDriverOptionsKeyPrefixed)) {
    parser_map[kChromeDriverOptionsKeyPrefixed] =
        base::BindRepeating(&ParseChromeOptions);
  } else {
    parser_map[kChromeDriverOptionsKey] =
        base::BindRepeating(&ParseChromeOptions);
  }
  // se:options.loggingPrefs and goog:loggingPrefs is spec-compliant name,
  // but loggingPrefs is still supported in legacy mode.
  const std::string prefixed_logging_prefs_key =
      base::StringPrintf("%s:loggingPrefs", kChromeDriverCompanyPrefix);
  if (desired_caps.FindDictByDottedPath("se:options.loggingPrefs")) {
    parser_map["se:options"] = base::BindRepeating(&ParseSeleniumOptions);
  } else if (w3c_compliant ||
             desired_caps.FindDictByDottedPath(prefixed_logging_prefs_key)) {
    parser_map[prefixed_logging_prefs_key] =
        base::BindRepeating(&ParseLoggingPrefs);
  } else {
    parser_map["loggingPrefs"] = base::BindRepeating(&ParseLoggingPrefs);
  }
  // Network emulation requires device mode, which is only enabled when
  // mobile emulation is on.

  const base::Value::Dict* chrome_options = nullptr;
  if (GetChromeOptionsDictionary(desired_caps, &chrome_options) &&
      chrome_options->FindDict("mobileEmulation")) {
    parser_map["networkConnectionEnabled"] =
        base::BindRepeating(&ParseBoolean, &network_emulation_enabled);
  }

  for (const auto item : desired_caps) {
    if (item.second.is_none())
      continue;
    if (parser_map.find(item.first) == parser_map.end()) {
      // The specified capability is unrecognized. W3C spec requires us to
      // return an error if capability does not contain ":".
      // In legacy mode, for backward compatibility reasons,
      // we ignore unrecognized capabilities.
      if (w3c_compliant && item.first.find(':') == std::string::npos) {
        return Status(kInvalidArgument,
                      "unrecognized capability: " + item.first);
      }
      continue;
    }
    Status status = parser_map[item.first].Run(item.second, this);
    if (status.IsError()) {
      return Status(kInvalidArgument, "cannot parse capability: " + item.first,
                    status);
    }
  }
  // Perf log must be enabled if perf log prefs are specified; otherwise, error.
  LoggingPrefs::const_iterator iter = logging_prefs.find(
      WebDriverLog::kPerformanceType);
  if (iter == logging_prefs.end() || iter->second == Log::kOff) {
    if (GetChromeOptionsDictionary(desired_caps, &chrome_options) &&
        chrome_options->Find("perfLoggingPrefs")) {
      return Status(kInvalidArgument,
                    "perfLoggingPrefs specified, "
                    "but performance logging was not enabled");
    }
  }
  LoggingPrefs::const_iterator dt_events_logging_iter = logging_prefs.find(
      WebDriverLog::kDevToolsType);
  if (dt_events_logging_iter == logging_prefs.end()
      || dt_events_logging_iter->second == Log::kOff) {
    if (GetChromeOptionsDictionary(desired_caps, &chrome_options) &&
        chrome_options->Find("devToolsEventsToLog")) {
      return Status(kInvalidArgument,
                    "devToolsEventsToLog specified, "
                    "but devtools events logging was not enabled");
    }
  }
  return Status(kOk);
}
