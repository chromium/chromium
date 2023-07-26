// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_mojom_traits.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"

namespace mojo {

// static
bool StructTraits<content_settings::mojom::PatternPartsDataView,
                  ContentSettingsPattern::PatternParts>::
    Read(content_settings::mojom::PatternPartsDataView data,
         ContentSettingsPattern::PatternParts* out) {
  out->is_scheme_wildcard = data.is_scheme_wildcard();
  out->has_domain_wildcard = data.has_domain_wildcard();
  out->is_port_wildcard = data.is_port_wildcard();
  out->is_path_wildcard = data.is_path_wildcard();
  return data.ReadScheme(&out->scheme) && data.ReadHost(&out->host) &&
         data.ReadPort(&out->port) && data.ReadPath(&out->path);
}

// static
bool StructTraits<content_settings::mojom::ContentSettingsPatternDataView,
                  ContentSettingsPattern>::
    Read(content_settings::mojom::ContentSettingsPatternDataView data,
         ContentSettingsPattern* out) {
  out->is_valid_ = data.is_valid();
  return data.ReadParts(&out->parts_);
}

// static
content_settings::mojom::ContentSetting
EnumTraits<content_settings::mojom::ContentSetting, ContentSetting>::ToMojom(
    ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_DEFAULT:
      return content_settings::mojom::ContentSetting::DEFAULT;
    case CONTENT_SETTING_ALLOW:
      return content_settings::mojom::ContentSetting::ALLOW;
    case CONTENT_SETTING_BLOCK:
      return content_settings::mojom::ContentSetting::BLOCK;
    case CONTENT_SETTING_ASK:
      return content_settings::mojom::ContentSetting::ASK;
    case CONTENT_SETTING_SESSION_ONLY:
      return content_settings::mojom::ContentSetting::SESSION_ONLY;
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
      return content_settings::mojom::ContentSetting::DETECT_IMPORTANT_CONTENT;
    case CONTENT_SETTING_NUM_SETTINGS:
      // CONTENT_SETTING_NUM_SETTINGS is a dummy enum value.
      break;
  }
  NOTREACHED();
  return content_settings::mojom::ContentSetting::DEFAULT;
}

// static
bool EnumTraits<content_settings::mojom::ContentSetting, ContentSetting>::
    FromMojom(content_settings::mojom::ContentSetting setting,
              ContentSetting* out) {
  switch (setting) {
    case content_settings::mojom::ContentSetting::DEFAULT:
      *out = CONTENT_SETTING_DEFAULT;
      return true;
    case content_settings::mojom::ContentSetting::ALLOW:
      *out = CONTENT_SETTING_ALLOW;
      return true;
    case content_settings::mojom::ContentSetting::BLOCK:
      *out = CONTENT_SETTING_BLOCK;
      return true;
    case content_settings::mojom::ContentSetting::ASK:
      *out = CONTENT_SETTING_ASK;
      return true;
    case content_settings::mojom::ContentSetting::SESSION_ONLY:
      *out = CONTENT_SETTING_SESSION_ONLY;
      return true;
    case content_settings::mojom::ContentSetting::DETECT_IMPORTANT_CONTENT:
      *out = CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;
      return true;
  }
  return false;
}

// static
content_settings::mojom::SessionModel EnumTraits<
    content_settings::mojom::SessionModel,
    content_settings::SessionModel>::ToMojom(content_settings::SessionModel
                                                 model) {
  switch (model) {
    case content_settings::SessionModel::Durable:
      return content_settings::mojom::SessionModel::DURABLE;
    case content_settings::SessionModel::UserSession:
      return content_settings::mojom::SessionModel::USER_SESSION;
    case content_settings::SessionModel::NonRestorableUserSession:
      return content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION;
    case content_settings::SessionModel::OneTime:
      return content_settings::mojom::SessionModel::ONE_TIME;
  }
}

// static
bool EnumTraits<content_settings::mojom::SessionModel,
                content_settings::SessionModel>::
    FromMojom(content_settings::mojom::SessionModel model,
              content_settings::SessionModel* out) {
  switch (model) {
    case content_settings::mojom::SessionModel::DURABLE:
      *out = content_settings::SessionModel::Durable;
      return true;
    case content_settings::mojom::SessionModel::USER_SESSION:
      *out = content_settings::SessionModel::UserSession;
      return true;
    case content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION:
      *out = content_settings::SessionModel::NonRestorableUserSession;
      return true;
    case content_settings::mojom::SessionModel::ONE_TIME:
      *out = content_settings::SessionModel::OneTime;
      return true;
  }
}

// static
bool StructTraits<content_settings::mojom::RuleMetaDataDataView,
                  content_settings::RuleMetaData>::
    Read(content_settings::mojom::RuleMetaDataDataView data,
         content_settings::RuleMetaData* out) {
  base::Time expiration;
  base::TimeDelta lifetime;
  if (!data.ReadExpiration(&expiration) || !data.ReadLifetime(&lifetime)) {
    return false;
  }
  if (lifetime.is_zero() != expiration.is_null() ||
      lifetime < base::TimeDelta()) {
    return false;
  }
  out->SetExpirationAndLifetime(expiration, lifetime);

  return data.ReadLastModified(&out->last_modified_) &&
         data.ReadLastUsed(&out->last_used_) &&
         data.ReadLastVisited(&out->last_visited_) &&
         data.ReadSessionModel(&out->session_model_);
}

// static
bool StructTraits<content_settings::mojom::ContentSettingPatternSourceDataView,
                  ContentSettingPatternSource>::
    Read(content_settings::mojom::ContentSettingPatternSourceDataView data,
         ContentSettingPatternSource* out) {
  out->incognito = data.incognito();
  return data.ReadPrimaryPattern(&out->primary_pattern) &&
         data.ReadSecondaryPattern(&out->secondary_pattern) &&
         data.ReadSettingValue(&out->setting_value) &&
         data.ReadMetadata(&out->metadata) && data.ReadSource(&out->source);
}

// static
bool StructTraits<content_settings::mojom::RendererContentSettingRulesDataView,
                  RendererContentSettingRules>::
    Read(content_settings::mojom::RendererContentSettingRulesDataView data,
         RendererContentSettingRules* out) {
  return data.ReadImageRules(&out->image_rules) &&
         data.ReadScriptRules(&out->script_rules) &&
         data.ReadPopupRedirectRules(&out->popup_redirect_rules) &&
         data.ReadMixedContentRules(&out->mixed_content_rules) &&
         data.ReadAutoDarkContentRules(&out->auto_dark_content_rules);
}

}  // namespace mojo
