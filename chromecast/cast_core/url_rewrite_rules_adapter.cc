// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/url_rewrite_rules_adapter.h"

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace chromecast {
namespace {

constexpr char kQueryParamReplacePrefix[] = "${";
constexpr char kQueryParamDeletePrefix[] = "${~";
constexpr char kQueryParamSuffix[] = "}";

// If there's a match, extracts an internal token name from |pattern|.  For
// example, extracts "example" from  "${example}".  |prefix| is either "${" or
// "${~".  Returns an empty StringPiece if there's no match.
base::StringPiece MaybeExtractTokenName(const std::string& pattern,
                                        base::StringPiece prefix) {
  if (base::StartsWith(pattern, prefix) &&
      base::EndsWith(pattern, kQueryParamSuffix)) {
    return base::MakeStringPiece(pattern.begin() + prefix.length(),
                                 pattern.end() - sizeof(kQueryParamSuffix) + 1);
  }
  return {};
}

base::StringPiece MaybeExtractReplaceTokenName(const std::string& pattern) {
  return MaybeExtractTokenName(pattern, kQueryParamReplacePrefix);
}

base::StringPiece MaybeExtractDeleteTokenName(const std::string& pattern) {
  return MaybeExtractTokenName(pattern, kQueryParamDeletePrefix);
}

struct ParamRuleBuilder {
  bool replace_removes_header{false};
  std::string replace_token;
  bool suppress_removes_header{false};
  std::string suppress_token;
  std::string initial_value;
};

struct RuleTranslationErrorLog {
  std::vector<std::string> warnings;
};

#define RULE_WARNING(errors) \
  StringStreamIntoErrors(errors, __FILE__, __LINE__).stream()

struct StringStreamIntoErrors {
  StringStreamIntoErrors(RuleTranslationErrorLog* errors,
                         const char* file,
                         int line)
      : errors(errors) {
    ss << file << ":" << line << ": ";
  }
  ~StringStreamIntoErrors() { errors->warnings.push_back(ss.str()); }

  std::ostream& stream() { return ss; }

  RuleTranslationErrorLog* errors;
  std::stringstream ss;
};

// Adds host filters to either |wildcard_host_names| or |full_host_names| in
// |translated_rules| depending on whether each hostname starts with "*.".
void HandleHostFilters(const cast::v2::UrlRequestRewriteRule& rule,
                       TranslatedRewriteRules* translated_rules) {
  for (auto& host_filter : rule.host_filters()) {
    if (host_filter.size()) {
      if (base::StartsWith(host_filter, "*.")) {
        translated_rules->wildcard_host_names.push_back(host_filter.substr(1));
        translated_rules->full_host_names.push_back(host_filter.substr(2));
      } else {
        translated_rules->full_host_names.push_back(host_filter);
      }
    }
  }
}

// Handles a remove-header rule by checking if it corresponds to a delete token
// or replace token, and if so adds it to the corresponding rule in
// |param_rules|.
void HandleRemoveHeader(
    const cast::v2::UrlRequestRewriteRemoveHeader& remove_header,
    base::flat_map<std::string, ParamRuleBuilder>* param_rules,
    RuleTranslationErrorLog* errors) {
  base::StringPiece delete_match =
      MaybeExtractDeleteTokenName(remove_header.query_pattern());
  if (!delete_match.empty()) {
    if (delete_match == remove_header.header_name()) {
      ParamRuleBuilder& param = (*param_rules)[remove_header.header_name()];
      if (param.suppress_removes_header) {
        RULE_WARNING(errors)
            << "Two remove-header rules for same suppress token: "
            << remove_header.query_pattern();
      }
      param.suppress_removes_header = true;
      return;
    }
    RULE_WARNING(errors)
        << "Delete pattern matched remove-header rule, but the "
           "extracted name did not match header_name: '"
        << delete_match << "' vs. '" << remove_header.header_name() << "'";
    return;
  }

  base::StringPiece replace_match =
      MaybeExtractReplaceTokenName(remove_header.query_pattern());
  if (!replace_match.empty()) {
    if (replace_match == remove_header.header_name()) {
      ParamRuleBuilder& param = (*param_rules)[remove_header.header_name()];
      if (param.replace_removes_header) {
        RULE_WARNING(errors)
            << "Two remove-header rules for same replace token: "
            << remove_header.query_pattern();
      }
      param.replace_removes_header = true;
      return;
    }
    RULE_WARNING(errors)
        << "Replace pattern matched remove-header rule, but the "
           "extracted name did not match header_name: '"
        << replace_match << "' vs. '" << remove_header.header_name() << "'";
    return;
  }

  RULE_WARNING(errors) << "Unrecognized remove-header query pattern: "
                       << remove_header.query_pattern();
}

// Handles a substitute-query rule by checking if it corresponds to a delete
// token or replace token, and if so adds it to the corresponding rule in
// |param_rules|.
void HandleSubstituteQueryPattern(
    const cast::v2::UrlRequestRewriteSubstituteQueryPattern& query_pattern,
    base::flat_map<std::string, ParamRuleBuilder>* param_rules,
    RuleTranslationErrorLog* errors) {
  if (query_pattern.pattern() == "&&" && query_pattern.substitution() == "&") {
    // NOTE: This is a consequence of modifying the query string, but
    // IdentificationSettingsManager should handle this for us.
    return;
  }
  base::StringPiece delete_match =
      MaybeExtractDeleteTokenName(query_pattern.pattern());
  if (!delete_match.empty()) {
    ParamRuleBuilder& param = (*param_rules)[std::string(delete_match)];
    if (!query_pattern.substitution().empty()) {
      RULE_WARNING(errors)
          << "Suppression token matched but has non-empty substitution "
             "string: "
          << query_pattern.substitution();
    }
    param.suppress_token = query_pattern.pattern();
  } else {
    base::StringPiece replace_match =
        MaybeExtractReplaceTokenName(query_pattern.pattern());
    if (!replace_match.empty()) {
      ParamRuleBuilder& param = (*param_rules)[std::string(replace_match)];
      if (!param.initial_value.empty()) {
        RULE_WARNING(errors)
            << "Multiple initial values for parameter '" << replace_match
            << "': '" << param.initial_value << "' and '"
            << query_pattern.substitution() << "'";
      }
      param.replace_token = query_pattern.pattern();
      param.initial_value = query_pattern.substitution();
    } else {
      RULE_WARNING(errors) << "Unrecognized substitute-pattern pattern: "
                           << query_pattern.pattern();
    }
  }
}

// Handles a UrlRequestRewrite rule which should correspond to a
// SubstitutableParameter.  It can either be a remove-header or substitute-query
// rule.
void HandleSubstitutableParamRewrite(
    const cast::v2::UrlRequestRewrite& rewrite,
    TranslatedRewriteRules* translated_rules,
    base::flat_map<std::string, ParamRuleBuilder>* param_rules,
    RuleTranslationErrorLog* errors) {
  switch (rewrite.action_case()) {
    case cast::v2::UrlRequestRewrite::kRemoveHeader:
      HandleRemoveHeader(rewrite.remove_header(), param_rules, errors);
      break;
    case cast::v2::UrlRequestRewrite::kSubstituteQueryPattern:
      HandleSubstituteQueryPattern(rewrite.substitute_query_pattern(),
                                   param_rules, errors);
      break;
    default:
      RULE_WARNING(errors)
          << "Unsupported rewrite rule encountered w/ host and "
             "scheme filter: "
          << rewrite.action_case();
      break;
  }
}

// Handles a list of rewrites (provided in |rule|) which should provide
// SubstitutableParameters.
void HandleSubstitutableParamRule(
    const cast::v2::UrlRequestRewriteRule& rule,
    TranslatedRewriteRules* translated_rules,
    base::flat_map<std::string, std::string>* base_headers,
    base::flat_map<std::string, ParamRuleBuilder>* param_rules,
    RuleTranslationErrorLog* errors) {
  if (rule.scheme_filters(0) != "https") {
    RULE_WARNING(errors) << "Received non-https scheme: "
                         << rule.scheme_filters(0);
    return;
  }
  if (!translated_rules->full_host_names.empty() ||
      !translated_rules->wildcard_host_names.empty()) {
    RULE_WARNING(errors) << "Multiple rules specifying host-filters";
    return;
  }
  if (rule.rewrites_size() == 0) {
    return;
  }

  if (!rule.rewrites(0).has_add_headers()) {
    RULE_WARNING(errors)
        << "Expected add-headers as first rule w/ host and scheme filter";
    return;
  }
  for (auto& header : rule.rewrites(0).add_headers().headers()) {
    base_headers->emplace(header.name(), header.value());
  }

  HandleHostFilters(rule, translated_rules);

  for (int i = 1; i < rule.rewrites_size(); ++i) {
    HandleSubstitutableParamRewrite(rule.rewrites(i), translated_rules,
                                    param_rules, errors);
  }
}

// Handles a list of rewrites (provided in |rule|) which should define some or
// all of AppSettings and DeviceSettings.  These can include add-headers or
// replace-url rewrites.
void HandleAppOrDeviceSettingRule(const cast::v2::UrlRequestRewriteRule& rule,
                                  TranslatedRewriteRules* translated_rules,
                                  RuleTranslationErrorLog* errors) {
  for (auto& rewrite : rule.rewrites()) {
    if (rewrite.has_add_headers()) {
      for (const auto& header : rewrite.add_headers().headers()) {
        // NOTE: Cast Core explicitly adds this but we don't want to mark it as
        // CORS-exempt, which IdentificationSettingsManager does.  For the Web
        // Runtime, it is already added to all requests.
        if (header.name() == "User-Agent") {
          return;
        }
        auto emplace_result = translated_rules->static_headers.emplace(
            header.name(), header.value());
        if (!emplace_result.second) {
          RULE_WARNING(errors)
              << "static header emplace failed: " << header.name();
        }
      }
    } else if (rewrite.has_replace_url()) {
      auto emplace_result = translated_rules->url_replacements.emplace(
          rewrite.replace_url().url_ends_with(),
          rewrite.replace_url().new_url());
      if (!emplace_result.second) {
        RULE_WARNING(errors) << "replace-url emplace failed: "
                             << rewrite.replace_url().url_ends_with();
      }
    } else {
      RULE_WARNING(errors)
          << "Unsupported rewrite rule encountered w/o host and "
             "scheme filter";
    }
  }
}

// Handles a top-level rule by deciding whether it's a SubstitutableParameter
// rule, an App/DeviceSettings rule, or unsupported.
void HandleRule(const cast::v2::UrlRequestRewriteRule& rule,
                TranslatedRewriteRules* translated_rules,
                base::flat_map<std::string, std::string>* base_headers,
                base::flat_map<std::string, ParamRuleBuilder>* param_rules,
                RuleTranslationErrorLog* errors) {
  if (rule.action() == cast::v2::UrlRequestRewriteRule_UrlRequestAction_DENY) {
    RULE_WARNING(errors) << "Rewrite action is DENY which is not supported";
    return;
  }

  if (rule.host_filters_size() >= 1 && rule.scheme_filters_size() == 1) {
    HandleSubstitutableParamRule(rule, translated_rules, base_headers,
                                 param_rules, errors);
  } else if (rule.host_filters_size() == 0 && rule.scheme_filters_size() == 0) {
    HandleAppOrDeviceSettingRule(rule, translated_rules, errors);
  } else {
    RULE_WARNING(errors) << "Unsupported host/scheme filter counts: ("
                         << rule.host_filters_size() << ", "
                         << rule.scheme_filters_size() << ")";
  }
}

// Converts valid rules from |param_rules| into final ParamRule structs by
// checking that the rules are complete and they correspond to a header in
// |base_headers|.
std::vector<ParamRule> VerifySubstitutableParamRules(
    const base::flat_map<std::string, std::string>& base_headers,
    base::flat_map<std::string, ParamRuleBuilder>* param_rules,
    RuleTranslationErrorLog* errors) {
  std::vector<ParamRule> result;

  for (auto& entry : *param_rules) {
    if (entry.second.replace_token.empty() ||
        !entry.second.replace_removes_header ||
        entry.second.suppress_token.empty() ||
        !entry.second.suppress_removes_header) {
      RULE_WARNING(errors) << "Parameter rule for '" << entry.first
                           << "' is incomplete";
      continue;
    }

    auto header_entry = base_headers.find(entry.first);
    if (header_entry == base_headers.end() ||
        header_entry->second != entry.second.initial_value) {
      RULE_WARNING(errors) << "Parameter rule for '" << entry.first
                           << "' has inconsistent value: '"
                           << entry.second.initial_value << "' vs. '"
                           << header_entry->second << "'";
      continue;
    }

    ParamRule param;
    param.name = entry.first;
    param.replace_token = std::move(entry.second.replace_token);
    param.suppress_token = std::move(entry.second.suppress_token);
    param.value = std::move(entry.second.initial_value);
    result.push_back(std::move(param));
  }
  return result;
}

// Converts a set of rewrite rules from gRPC to a form more directly suitable
// for using with IdentificationSettingsManager.
TranslatedRewriteRules TranslateRewriteRules(
    const cast::v2::UrlRequestRewriteRules& rules) {
  TranslatedRewriteRules result;
  base::flat_map<std::string, std::string> base_headers;
  base::flat_map<std::string, ParamRuleBuilder> param_rules;
  RuleTranslationErrorLog errors;

  for (auto& rule : rules.rules()) {
    HandleRule(rule, &result, &base_headers, &param_rules, &errors);
  }

  result.params =
      VerifySubstitutableParamRules(base_headers, &param_rules, &errors);

  if (!errors.warnings.empty()) {
    DLOG(WARNING) << "Warnings while translating URL rewrite rules:";
    for (const auto& message : errors.warnings) {
      DLOG(WARNING) << message;
    }
  }

  return result;
}

// Computes IndexValuePair updates for SubstitutableParameters by matching new
// rules received from gRPC with existing rules.
absl::optional<std::vector<mojom::IndexValuePairPtr>> ComputeUpdatedParams(
    const std::vector<ParamRule>& current_params,
    const std::vector<ParamRule>& updated_params) {
  std::vector<mojom::IndexValuePairPtr> result;
  for (const auto& param : updated_params) {
    auto it = std::find_if(
        current_params.begin(), current_params.end(),
        [&param](const ParamRule& test) { return test.name == param.name; });
    if (it == current_params.end()) {
      DLOG(WARNING)
          << "Got a new parameter name when we should only get updates: "
          << param.name;
      return absl::nullopt;
    }
    DCHECK_EQ(it->replace_token, param.replace_token);
    DCHECK_EQ(it->suppress_token, param.suppress_token);
    if (it->value != param.value) {
      mojom::IndexValuePairPtr pair = mojom::IndexValuePair::New();
      pair->index = it - current_params.begin();
      pair->value = param.value;
      result.push_back(std::move(pair));
    }
  }
  return result;
}

std::vector<mojom::SubstitutableParameterPtr> ConvertParamsToMojo(
    const TranslatedRewriteRules& rules) {
  std::vector<mojom::SubstitutableParameterPtr> result;
  for (const auto& param : rules.params) {
    mojom::SubstitutableParameterPtr param_ptr =
        mojom::SubstitutableParameter::New();
    param_ptr->name = param.name;
    param_ptr->replacement_token = param.replace_token;
    param_ptr->suppression_token = param.suppress_token;
    param_ptr->is_signature = false;
    param_ptr->value = param.value;
    result.push_back(std::move(param_ptr));
  }
  return result;
}

mojom::AppSettingsPtr ConvertAppSettingsToMojo(
    const TranslatedRewriteRules& rules) {
  mojom::AppSettingsPtr result = mojom::AppSettings::New();
  result->allowed_headers = (1u << rules.params.size()) - 1;
  // TODO(btolsch): How to determine this?
  result->allowed_for_device_identification = true;
  for (const auto& host_name : rules.full_host_names) {
    result->full_host_names.push_back(host_name);
  }
  for (const auto& host_name : rules.wildcard_host_names) {
    result->wildcard_host_names.push_back(host_name);
  }
  return result;
}

mojom::DeviceSettingsPtr ConvertDeviceSettingsToMojo(
    const TranslatedRewriteRules& rules) {
  mojom::DeviceSettingsPtr result = mojom::DeviceSettings::New();
  for (const auto& header : rules.static_headers) {
    result->static_headers.emplace(header.first, header.second);
  }
  for (const auto& entry : rules.url_replacements) {
    result->url_replacements.emplace(GURL(entry.first), GURL(entry.second));
  }
  return result;
}

}  // namespace

MojoIdentificationSettings::MojoIdentificationSettings(
    const cast::v2::UrlRequestRewriteRules& rules) {
  auto translated_rules = TranslateRewriteRules(rules);
  substitutable_params = ConvertParamsToMojo(translated_rules);
  application_settings = ConvertAppSettingsToMojo(translated_rules);
  device_settings = ConvertDeviceSettingsToMojo(translated_rules);
}
MojoIdentificationSettings::~MojoIdentificationSettings() = default;

ParamRule::ParamRule() = default;
ParamRule::~ParamRule() = default;
ParamRule::ParamRule(ParamRule&&) = default;
ParamRule& ParamRule::operator=(ParamRule&&) = default;

TranslatedRewriteRules::TranslatedRewriteRules() = default;
TranslatedRewriteRules::~TranslatedRewriteRules() = default;
TranslatedRewriteRules::TranslatedRewriteRules(TranslatedRewriteRules&&) =
    default;
TranslatedRewriteRules& TranslatedRewriteRules::operator=(
    TranslatedRewriteRules&&) = default;

UrlRewriteRulesAdapter::UrlRewriteRulesAdapter(
    const cast::v2::UrlRequestRewriteRules& rules) {
  DCHECK(remote_settings_managers_.empty());
  rules_ = TranslateRewriteRules(rules);
}

UrlRewriteRulesAdapter::~UrlRewriteRulesAdapter() = default;

void UrlRewriteRulesAdapter::UpdateRules(
    const cast::v2::UrlRequestRewriteRules& rules) {
  TranslatedRewriteRules translated = TranslateRewriteRules(rules);
  auto update_params = ComputeUpdatedParams(rules_.params, translated.params);
  if (!update_params) {
    return;
  }

  rules_ = std::move(translated);
  mojom::AppSettingsPtr app_settings_mojo = ConvertAppSettingsToMojo(rules_);
  mojom::DeviceSettingsPtr device_settings_mojo =
      ConvertDeviceSettingsToMojo(rules_);

  for (auto& entry : remote_settings_managers_) {
    std::vector<mojom::IndexValuePairPtr> param_update_clone;
    param_update_clone.reserve(update_params.value().size());
    for (const auto& param_update : update_params.value()) {
      param_update_clone.push_back(param_update->Clone());
    }
    mojo::AssociatedRemote<mojom::IdentificationSettingsManager>&
        settings_manager = entry.settings_manager;
    settings_manager->UpdateSubstitutableParamValues(
        std::move(param_update_clone));
    settings_manager->UpdateDeviceSettings(device_settings_mojo->Clone());
    settings_manager->UpdateAppSettings(app_settings_mojo->Clone());
  }
}

void UrlRewriteRulesAdapter::AddRenderFrame(
    mojo::AssociatedRemote<mojom::IdentificationSettingsManager>
        remote_settings_manager) {
  remote_settings_manager.set_disconnect_handler(base::BindOnce(
      &UrlRewriteRulesAdapter::OnRenderFrameRemoved, weak_factory_.GetWeakPtr(),
      base::Unretained(remote_settings_manager.get())));

  mojo::PendingRemote<mojom::ClientAuthDelegate> client_auth_delegate;
  mojo::ReceiverId delegate_id = auth_delegate_bindings_.Add(
      this, client_auth_delegate.InitWithNewPipeAndPassReceiver());
  remote_settings_manager->SetClientAuth(std::move(client_auth_delegate));

  std::vector<mojom::SubstitutableParameterPtr> params_mojo =
      ConvertParamsToMojo(rules_);
  mojom::AppSettingsPtr app_settings_mojo = ConvertAppSettingsToMojo(rules_);
  mojom::DeviceSettingsPtr device_settings_mojo =
      ConvertDeviceSettingsToMojo(rules_);

  std::vector<mojom::SubstitutableParameterPtr> params_clone;
  params_clone.reserve(params_mojo.size());
  for (const auto& param : params_mojo) {
    params_clone.push_back(param->Clone());
  }
  remote_settings_manager->SetSubstitutableParameters(std::move(params_clone));
  remote_settings_manager->UpdateDeviceSettings(device_settings_mojo->Clone());
  remote_settings_manager->UpdateAppSettings(app_settings_mojo->Clone());

  FrameInfo frame_info;
  frame_info.settings_manager = std::move(remote_settings_manager);
  frame_info.auth_delegate_id = delegate_id;
  remote_settings_managers_.insert(std::move(frame_info));
}

UrlRewriteRulesAdapter::FrameInfo::FrameInfo() = default;
UrlRewriteRulesAdapter::FrameInfo::~FrameInfo() = default;

UrlRewriteRulesAdapter::FrameInfo::FrameInfo(FrameInfo&&) = default;
UrlRewriteRulesAdapter::FrameInfo& UrlRewriteRulesAdapter::FrameInfo::operator=(
    FrameInfo&&) = default;

bool UrlRewriteRulesAdapter::FrameInfo::operator<(
    const FrameInfo& other) const {
  return settings_manager.get() < other.settings_manager.get();
}

void UrlRewriteRulesAdapter::OnRenderFrameRemoved(
    mojom::IdentificationSettingsManagerProxy* settings_manager_proxy) {
  auto entry = std::find_if(
      remote_settings_managers_.begin(), remote_settings_managers_.end(),
      [settings_manager_proxy](const FrameInfo& frame_info) {
        return frame_info.settings_manager.get() == settings_manager_proxy;
      });
  if (entry != remote_settings_managers_.end()) {
    auth_delegate_bindings_.Remove(entry->auth_delegate_id);
    remote_settings_managers_.erase(entry);
  }
}

void UrlRewriteRulesAdapter::EnsureCerts(EnsureCertsCallback callback) {
  DCHECK(false);
  std::move(callback).Run({});
}

void UrlRewriteRulesAdapter::EnsureSignature(EnsureSignatureCallback callback) {
  DCHECK(false);
  std::move(callback).Run({}, base::Time::Max());
}

}  // namespace chromecast
