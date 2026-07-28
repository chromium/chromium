// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/payment_manifest_parser.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/payments/content/utility/fingerprint_parser.h"
#include "components/payments/core/error_logger.h"
#include "components/payments/core/url_util.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

namespace payments {
namespace {

constexpr size_t kMaximumNumberOfItems = 100U;
constexpr size_t kMaximumNumberOfSupportedOrigins = 100000;
constexpr size_t kMaximumNumberOfSupportedDelegations = 4U;
constexpr size_t kMaximumPrintedStringLength = 100U;

constexpr char kDefaultApplications[] = "default_applications";
constexpr char kFingerprints[] = "fingerprints";
constexpr char kHttpPrefix[] = "http://";
constexpr char kHttpsPrefix[] = "https://";
constexpr char kId[] = "id";
constexpr char kMinVersion[] = "min_version";
constexpr char kPayment[] = "payment";
constexpr char kPlatform[] = "platform";
constexpr char kPlay[] = "play";
constexpr char kPreferRelatedApplications[] = "prefer_related_applications";
constexpr char kRelatedApplications[] = "related_applications";
constexpr char kServiceWorkerScope[] = "scope";
constexpr char kServiceWorker[] = "serviceworker";
constexpr char kServiceWorkerSrc[] = "src";
constexpr char kServiceWorkerUseCache[] = "use_cache";
constexpr char kSupportedDelegations[] = "supported_delegations";
constexpr char kSupportedOrigins[] = "supported_origins";
constexpr char kWebAppIcons[] = "icons";
constexpr char kWebAppIconSizes[] = "sizes";
constexpr char kWebAppIconSrc[] = "src";
constexpr char kWebAppIconType[] = "type";
constexpr char kWebAppName[] = "name";

// Truncates a std::string to 100 chars. This returns an empty string when the
// input should be printalbe ASCII but it's not.
const std::string ValidateAndTruncateIfNeeded(const std::string& input,
                                              bool* out_is_ascii_printable) {
  if (out_is_ascii_printable) {
    *out_is_ascii_printable = std::ranges::all_of(
        input, [](char c) { return base::IsAsciiPrintable(c); });
    if (!*out_is_ascii_printable) {
      return "";
    }
  }

  return input.size() > kMaximumPrintedStringLength
             ? (input.substr(0, kMaximumPrintedStringLength) + "...")
             : input;
}

// Parses the "default_applications": ["https://some/url"] from |dict| into
// |web_app_manifest_urls|. Uses |manifest_url| to resolve relative URLs.
// Returns 'false' for invalid data.
bool ParseDefaultApplications(const GURL& manifest_url,
                              const base::DictValue* dict,
                              std::vector<GURL>* web_app_manifest_urls,
                              const ErrorLogger& log) {
  DCHECK(dict);
  DCHECK(web_app_manifest_urls);

  const base::ListValue* list = dict->FindList(kDefaultApplications);
  if (!list) {
    // TODO(crbug.com/40681786): Move the error message strings to
    // components/payments/core/native_error_strings.cc.
    log.Error(
        base::StringPrintf("\"%s\" must be a list.", kDefaultApplications));
    return false;
  }

  size_t apps_number = list->size();
  if (apps_number > kMaximumNumberOfItems) {
    log.Error(base::StringPrintf("\"%s\" must contain at most %zu entries.",
                                 kDefaultApplications, kMaximumNumberOfItems));
    return false;
  }

  for (size_t i = 0; i < apps_number; ++i) {
    const std::string* item = (*list)[i].GetIfString();
    if (!item || item->empty() || !base::IsStringUTF8(*item)) {
      log.Error(base::StringPrintf("Each entry in \"%s\" must be UTF8 string.",
                                   kDefaultApplications));
      web_app_manifest_urls->clear();
      return false;
    }

    GURL url = manifest_url.Resolve(*item);
    // TODO(crbug.com/40681786): Check that |url| is the same origin with
    // |manifest_url|. Currently that's checked by callers, but the earlier this
    // is caught, the fewer resources Chrome consumes.
    if (!UrlUtil::IsValidManifestUrl(url)) {
      const std::string item_to_print =
          ValidateAndTruncateIfNeeded(*item, nullptr);
      log.Error(
          base::StringPrintf("\"%s\" entry in \"%s\" is not a valid URL with "
                             "HTTPS scheme and is "
                             "not a valid localhost URL with HTTP scheme.",
                             item_to_print.c_str(), kDefaultApplications));
      web_app_manifest_urls->clear();
      return false;
    }

    web_app_manifest_urls->push_back(url);
  }

  return true;
}

// Parses the "supported_origins": "https://some.origin" from |dict|
// into |supported_origins|. Returns 'false' for invalid data.
bool ParseSupportedOrigins(const base::DictValue* dict,
                           std::vector<url::Origin>* supported_origins,
                           const ErrorLogger& log) {
  DCHECK(dict);
  DCHECK(supported_origins);

  const base::ListValue* list = dict->FindList(kSupportedOrigins);
  if (!list) {
    log.Error(base::StringPrintf("\"%s\" must be a list of origins.",
                                 kSupportedOrigins));
    return false;
  }

  size_t supported_origins_number = list->size();
  if (supported_origins_number > kMaximumNumberOfSupportedOrigins) {
    log.Error(base::StringPrintf("\"%s\" must contain at most %zu entires.",
                                 kSupportedOrigins,
                                 kMaximumNumberOfSupportedOrigins));
    return false;
  }

  for (size_t i = 0; i < supported_origins_number; ++i) {
    const std::string* item = (*list)[i].GetIfString();
    if (!item || item->empty() || !base::IsStringUTF8(*item) ||
        !(base::StartsWith(*item, kHttpsPrefix, base::CompareCase::SENSITIVE) ||
          base::StartsWith(*item, kHttpPrefix, base::CompareCase::SENSITIVE))) {
      supported_origins->clear();
      log.Error(base::StringPrintf(
          "Each entry in \"%s\" must be UTF8 string that starts with \"%s\" or "
          "\"%s\" (for localhost).",
          kSupportedOrigins, kHttpsPrefix, kHttpPrefix));
      return false;
    }

    GURL url(*item);
    if (!UrlUtil::IsValidSupportedOrigin(url)) {
      supported_origins->clear();
      const std::string item_to_print =
          ValidateAndTruncateIfNeeded(*item, nullptr);
      log.Error(base::StringPrintf(
          "\"%s\" entry in \"%s\" is not a valid origin with HTTPS scheme "
          "and "
          "is not a valid localhost origin with HTTP scheme.",
          item_to_print.c_str(), kSupportedOrigins));
      return false;
    }

    supported_origins->push_back(url::Origin::Create(url));
  }

  return true;
}

void ParseIcons(const base::DictValue& dict,
                const ErrorLogger& log,
                std::vector<PaymentManifestParser::WebAppIcon>* icons) {
  DCHECK(icons);

  const base::ListValue* icons_list = dict.FindList(kWebAppIcons);
  if (!icons_list) {
    log.Warn(
        base::StringPrintf("No \"%s\" list in the manifest.", kWebAppIcons));
    return;
  }

  for (const auto& icon : *icons_list) {
    if (!icon.is_dict()) {
      log.Warn(base::StringPrintf(
          "Each item in the list \"%s\" should be a dictionary.",
          kWebAppIcons));
      continue;
    }

    PaymentManifestParser::WebAppIcon web_app_icon;
    const std::string* icon_src = icon.GetDict().FindString(kWebAppIconSrc);
    if (!icon_src || icon_src->empty() || !base::IsStringUTF8(*icon_src)) {
      log.Warn(
          base::StringPrintf("Each dictionary in the list \"%s\" should "
                             "contain a non-empty UTF8 string field \"%s\".",
                             kWebAppIcons, kWebAppIconSrc));
      continue;
    }
    web_app_icon.src = *icon_src;

    const std::string* icon_sizes = icon.GetDict().FindString(kWebAppIconSizes);
    if (icon_sizes) {
      if (icon_sizes->empty() || !base::IsStringUTF8(*icon_sizes)) {
        log.Warn(base::StringPrintf(
            "Each \"%s\" in \"%s\" should be a non-empty UTF8 string.",
            kWebAppIconSizes, kWebAppIcons));
      } else {
        web_app_icon.sizes = *icon_sizes;
      }
    }

    const std::string* icon_type = icon.GetDict().FindString(kWebAppIconType);
    if (icon_type) {
      if (icon_type->empty() || !base::IsStringUTF8(*icon_type)) {
        log.Warn(base::StringPrintf(
            "Each \"%s\" in \"%s\" should be a non-empty UTF8 string.",
            kWebAppIconType, kWebAppIcons));
      } else {
        web_app_icon.type = *icon_type;
      }
    }

    icons->emplace_back(web_app_icon);
  }
}

void ParsePreferredRelatedApplicationIdentifiers(
    const base::DictValue& dict,
    const ErrorLogger& log,
    std::vector<std::string>* ids) {
  DCHECK(ids);

  if (!dict.Find(kPreferRelatedApplications))
    return;

  std::optional<bool> prefer_related_applications =
      dict.FindBool(kPreferRelatedApplications);
  if (!prefer_related_applications.has_value()) {
    log.Warn(base::StringPrintf("The \"%s\" field should be a boolean.",
                                kPreferRelatedApplications));
    return;
  }

  if (!prefer_related_applications.value())
    return;

  const base::ListValue* related_applications =
      dict.FindList(kRelatedApplications);
  if (!related_applications) {
    log.Warn(
        base::StringPrintf("The \"%s\" field should be a list of dictionaries.",
                           kRelatedApplications));
    return;
  }

  size_t size = related_applications->size();
  if (size == 0) {
    log.Warn(base::StringPrintf(
        "Did not find any entries in \"%s\", even though \"%s\" is true.",
        kRelatedApplications, kPreferRelatedApplications));
    return;
  }

  for (size_t i = 0; i < size; ++i) {
    const base::Value& related_application_value = (*related_applications)[i];
    const base::DictValue* related_application =
        related_application_value.GetIfDict();
    if (!related_application) {
      log.Warn(
          base::StringPrintf("Element #%zu in \"%s\" should be a dictionary.",
                             i, kRelatedApplications));
      continue;
    }

    const std::string* platform = related_application->FindString(kPlatform);
    if (!platform || *platform != kPlay) {
      continue;
    }

    if (ids->size() >= kMaximumNumberOfItems) {
      log.Warn(base::StringPrintf(
          "The maximum number of items in \"%s\" with \"%s\":\"%s\" should not "
          "exceed %zu.",
          kRelatedApplications, kPlatform, kPlay, kMaximumNumberOfItems));
      break;
    }

    const std::string* id = related_application->FindString(kId);
    if (!id) {
      log.Warn(base::StringPrintf(
          "Elements in \"%s\" with \"%s\":\"%s\" should have \"%s\" field.",
          kRelatedApplications, kPlatform, kPlay, kId));
      continue;
    }

    if (id->empty() || !base::IsStringASCII(*id)) {
      log.Warn(base::StringPrintf(
          "\"%s\".\"%s\" should be a non-empty ASCII string.",
          kRelatedApplications, kId));
      continue;
    }

    ids->emplace_back(*id);
  }
}

bool GetString(const base::DictValue* dict,
               std::string_view key,
               std::string& result) {
  DCHECK(dict);
  const std::string* value = dict->FindString(key);
  if (value) {
    result = *value;
  }
  return value;
}

}  // namespace

PaymentManifestParser::WebAppIcon::WebAppIcon() = default;

PaymentManifestParser::WebAppIcon::~WebAppIcon() = default;

PaymentManifestParser::WebAppInstallationInfoResult::
    WebAppInstallationInfoResult() = default;
PaymentManifestParser::WebAppInstallationInfoResult::
    ~WebAppInstallationInfoResult() = default;
PaymentManifestParser::WebAppInstallationInfoResult::
    WebAppInstallationInfoResult(WebAppInstallationInfoResult&&) = default;
PaymentManifestParser::WebAppInstallationInfoResult&
PaymentManifestParser::WebAppInstallationInfoResult::operator=(
    WebAppInstallationInfoResult&&) = default;

PaymentManifestParser::PaymentManifestParser(std::unique_ptr<ErrorLogger> log)
    : log_(std::move(log)) {
  DCHECK(log_);
}

PaymentManifestParser::~PaymentManifestParser() = default;

bool PaymentManifestParser::ParsePaymentMethodManifest(
    const GURL& manifest_url,
    const std::string& content,
    std::vector<GURL>* web_app_manifest_urls,
    std::vector<url::Origin>* supported_origins) {
  DCHECK(web_app_manifest_urls);
  DCHECK(supported_origins);

  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(content,
                                                    base::JSON_PARSE_RFC);
  if (!result.has_value()) {
    log_->Error(result.error().message);
    return false;
  }

  const base::DictValue* dict = result->GetIfDict();
  if (!dict) {
    log_->Error("Payment method manifest must be a JSON dictionary.");
    return false;
  }

  ParsePaymentMethodManifestIntoVectors(
      manifest_url, *dict, *log_, web_app_manifest_urls, supported_origins);
  return true;
}

std::vector<WebAppManifestSection> PaymentManifestParser::ParseWebAppManifest(
    const std::string& content) {
  std::vector<WebAppManifestSection> manifest;
  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(content,
                                                    base::JSON_PARSE_RFC);
  if (!result.has_value()) {
    log_->Error(result.error().message);
    return manifest;
  }

  const base::DictValue* dict = result->GetIfDict();
  if (!dict) {
    log_->Error("Web app manifest must be a JSON dictionary.");
    return manifest;
  }

  ParseWebAppManifestIntoVector(*dict, *log_, &manifest);
  return manifest;
}

PaymentManifestParser::WebAppInstallationInfoResult
PaymentManifestParser::ParseWebAppInstallationInfo(const std::string& content) {
  WebAppInstallationInfoResult result;
  base::JSONReader::Result json_result =
      base::JSONReader::ReadAndReturnValueWithError(content,
                                                    base::JSON_PARSE_RFC);
  if (!json_result.has_value()) {
    log_->Error(json_result.error().message);
    return result;
  }

  const base::DictValue* dict = json_result->GetIfDict();
  if (!dict) {
    log_->Error("Web app manifest must be a JSON dictionary.");
    return result;
  }

  result.installation_info = std::make_unique<WebAppInstallationInfo>();
  result.icons = std::make_unique<std::vector<WebAppIcon>>();
  if (!ParseWebAppInstallationInfoIntoStructs(
          *dict, *log_, result.installation_info.get(), result.icons.get())) {
    result.installation_info.reset();
    result.icons.reset();
  }
  return result;
}

// static
void PaymentManifestParser::ParsePaymentMethodManifestIntoVectors(
    const GURL& manifest_url,
    const base::DictValue& dict,
    const ErrorLogger& log,
    std::vector<GURL>* web_app_manifest_urls,
    std::vector<url::Origin>* supported_origins) {
  DCHECK(web_app_manifest_urls);
  DCHECK(supported_origins);

  if (dict.Find(kDefaultApplications) &&
      !ParseDefaultApplications(manifest_url, &dict, web_app_manifest_urls,
                                log)) {
    return;
  }

  if (dict.Find(kSupportedOrigins) &&
      !ParseSupportedOrigins(&dict, supported_origins, log)) {
    web_app_manifest_urls->clear();
  }
}

// static
bool PaymentManifestParser::ParseWebAppManifestIntoVector(
    const base::DictValue& dict,
    const ErrorLogger& log,
    std::vector<WebAppManifestSection>* output) {
  const base::ListValue* list = dict.FindList(kRelatedApplications);
  if (!list) {
    log.Error(
        base::StringPrintf("\"%s\" must be a list.", kRelatedApplications));
    return false;
  }

  for (const base::Value& related_application_value : *list) {
    const base::DictValue* related_application =
        related_application_value.GetIfDict();
    if (!related_application) {
      log.Error(base::StringPrintf("\"%s\" must be a list of dictionaries.",
                                   kRelatedApplications));
      output->clear();
      return false;
    }

    const std::string* platform = related_application->FindString(kPlatform);
    if (!platform || *platform != kPlay) {
      continue;
    }

    if (output->size() >= kMaximumNumberOfItems) {
      log.Error(base::StringPrintf(
          "\"%s\" must contain at most %zu entries with \"%s\": \"%s\".",
          kRelatedApplications, kMaximumNumberOfItems, kPlatform, kPlay));
      output->clear();
      return false;
    }

    if (!related_application->Find(kId) ||
        !related_application->Find(kMinVersion) ||
        !related_application->Find(kFingerprints)) {
      log.Error(
          base::StringPrintf("Each \"%s\": \"%s\" entry in \"%s\" must contain "
                             "\"%s\", \"%s\", and \"%s\".",
                             kPlatform, kPlay, kRelatedApplications, kId,
                             kMinVersion, kFingerprints));
      return false;
    }

    WebAppManifestSection section;
    section.min_version = 0;

    const std::string* section_id = related_application->FindString(kId);
    if (!section_id || section_id->empty() ||
        !base::IsStringASCII(*section_id)) {
      log.Error(
          base::StringPrintf("\"%s\" must be a non-empty ASCII string.", kId));
      output->clear();
      return false;
    }
    section.id = *section_id;

    const std::string* min_version =
        related_application->FindString(kMinVersion);
    if (!min_version || min_version->empty() ||
        !base::IsStringASCII(*min_version) ||
        !base::StringToInt64(*min_version, &section.min_version)) {
      log.Error(base::StringPrintf(
          "\"%s\" must be a string convertible into a number.", kMinVersion));
      output->clear();
      return false;
    }

    const base::ListValue* fingerprints_list =
        related_application->FindList(kFingerprints);
    if (!fingerprints_list || fingerprints_list->empty() ||
        fingerprints_list->size() > kMaximumNumberOfItems) {
      log.Error(base::StringPrintf(
          "\"%s\" must be a non-empty list of at most %zu items.",
          kFingerprints, kMaximumNumberOfItems));
      output->clear();
      return false;
    }

    for (const base::Value& fingerprint_dict_value : *fingerprints_list) {
      const base::DictValue* fingerprint_dict =
          fingerprint_dict_value.GetIfDict();
      std::string fingerprint_type;
      std::string fingerprint_value;
      if (!fingerprint_dict ||
          !GetString(fingerprint_dict, "type", fingerprint_type) ||
          fingerprint_type != "sha256_cert" ||
          !GetString(fingerprint_dict, "value", fingerprint_value) ||
          fingerprint_value.empty() ||
          !base::IsStringASCII(fingerprint_value)) {
        log.Error(base::StringPrintf(
            "Each entry in \"%s\" must be a dictionary with \"type\": "
            "\"sha256_cert\" and a non-empty ASCII string \"value\".",
            kFingerprints));
        output->clear();
        return false;
      }

      std::vector<uint8_t> hash =
          FingerprintStringToByteArray(fingerprint_value, log);
      if (hash.empty()) {
        output->clear();
        return false;
      }

      section.fingerprints.push_back(hash);
    }

    output->push_back(section);
  }

  return true;
}

// static
bool PaymentManifestParser::ParseWebAppInstallationInfoIntoStructs(
    const base::DictValue& dict_ref,
    const ErrorLogger& log,
    WebAppInstallationInfo* installation_info,
    std::vector<WebAppIcon>* icons) {
  DCHECK(installation_info);
  DCHECK(icons);

  const base::DictValue* dict = &dict_ref;

  {
    const base::DictValue* service_worker_dict = dict->FindDict(kServiceWorker);
    if (!service_worker_dict) {
      log.Error(base::StringPrintf(
          "\"%s\" must be a dictionary in your web app manifest.",
          kServiceWorker));
      return false;
    }

    const std::string* sw_js_url =
        service_worker_dict->FindString(kServiceWorkerSrc);
    if (!sw_js_url || sw_js_url->empty() || !base::IsStringUTF8(*sw_js_url)) {
      log.Error(
          base::StringPrintf("\"%s\".\"%s\" must be a non-empty UTF8 string.",
                             kServiceWorker, kServiceWorkerSrc));
      return false;
    }
    installation_info->sw_js_url = *sw_js_url;

    const std::string* sw_scope =
        service_worker_dict->FindString(kServiceWorkerScope);
    if (sw_scope) {
      installation_info->sw_scope = *sw_scope;
    }

    std::optional<bool> use_cache =
        service_worker_dict->FindBool(kServiceWorkerUseCache);
    if (use_cache.has_value()) {
      installation_info->sw_use_cache = use_cache.value();
    }
  }

  const std::string* name = dict->FindString(kWebAppName);
  if (name) {
    installation_info->name = *name;
  }
  if (installation_info->name.empty()) {
    log.Warn(
        base::StringPrintf("No \"%s\" string in the manifest.", kWebAppName));
  }

  ParseIcons(*dict, log, icons);
  ParsePreferredRelatedApplicationIdentifiers(
      *dict, log, &installation_info->preferred_app_ids);

  const base::DictValue* payment_dict = dict->FindDict(kPayment);
  if (payment_dict) {
    const base::ListValue* delegation_list =
        payment_dict->FindList(kSupportedDelegations);
    if (delegation_list) {
      if (delegation_list->empty() ||
          delegation_list->size() > kMaximumNumberOfSupportedDelegations) {
        log.Error(base::StringPrintf(
            "\"%s.%s\" must be a non-empty list of at most %zu entries.",
            kPayment, kSupportedDelegations,
            kMaximumNumberOfSupportedDelegations));
        return false;
      }
      for (const auto& delegation_item : *delegation_list) {
        std::string delegation_name = delegation_item.GetString();
        if (delegation_name == "shippingAddress") {
          installation_info->supported_delegations.shipping_address = true;
        } else if (delegation_name == "payerName") {
          installation_info->supported_delegations.payer_name = true;
        } else if (delegation_name == "payerEmail") {
          installation_info->supported_delegations.payer_email = true;
        } else if (delegation_name == "payerPhone") {
          installation_info->supported_delegations.payer_phone = true;
        } else {  // delegation_name is not valid
          bool is_ascii_printable;
          const std::string delegation_name_to_print =
              ValidateAndTruncateIfNeeded(delegation_name, &is_ascii_printable);
          if (!is_ascii_printable) {
            log.Error(
                "Entries in delegation list must be printable ASCII strings.");
          } else {  // Printable ASCII string.
            log.Error(base::StringPrintf(
                "\"%s\" is not a valid value in \"%s\" array.",
                delegation_name_to_print.c_str(), kSupportedDelegations));
          }
          return false;
        }
      }
    } else {  // !payment_dict->GetList(kSupportedDelegations, &delegation_list)
      log.Error(base::StringPrintf("\"%s\" member must have \"%s\" list",
                                   kPayment, kSupportedDelegations));
      return false;
    }
  } else if (dict->Find(kPayment)) {
    log.Error(
        base::StringPrintf("\"%s\" member must be a dictionary", kPayment));
    return false;
  }

  return true;
}

}  // namespace payments
