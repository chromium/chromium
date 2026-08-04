// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_ai_model_cache_factory.h"
#include "chrome/browser/autofill/autofill_ai_personal_context_access_manager_factory.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/grit/autofill_and_password_manager_internals_resources.h"
#include "components/grit/autofill_and_password_manager_internals_resources_map.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_change_service_interface.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "components/webui/version/version_handler_helper.h"
#include "components/webui/version/version_ui_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"  // nogncheck
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#endif

namespace autofill {

void CreateAndAddInternalsHTMLSource(Profile* profile,
                                     const std::string& source_name) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, source_name);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  source->AddResourcePaths(kAutofillAndPasswordManagerInternalsResources);
  source->AddResourcePath(
      "",
      IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  // Data strings:
  source->AddString(version_ui::kVersion, version_info::GetVersionNumber());
  source->AddString(version_ui::kOfficial, version_info::IsOfficialBuild()
                                               ? "official"
                                               : "Developer build");
  source->AddString(version_ui::kVersionModifier,
                    chrome::GetChannelName(chrome::WithExtendedStable(true)));
  source->AddString(version_ui::kCL, version_info::GetLastChange());
  source->AddString(version_ui::kUserAgent, embedder_support::GetUserAgent());
  source->AddString("app_locale", g_browser_process->GetApplicationLocale());
}

AutofillCacheResetter::AutofillCacheResetter(
    content::BrowserContext* browser_context)
    : remover_(browser_context->GetBrowsingDataRemover()) {
  remover_->AddObserver(this);
}

AutofillCacheResetter::~AutofillCacheResetter() {
  remover_->RemoveObserver(this);
}

void AutofillCacheResetter::ResetCache(Callback callback) {
  if (callback_) {
    std::move(callback).Run(kCacheResetAlreadyInProgress);
    return;
  }

  callback_ = std::move(callback);

  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(
      url::Origin::Create(GURL("https://content-autofill.googleapis.com")));
  remover_->RemoveWithFilterAndReply(
      base::Time::Min(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      std::move(filter_builder), this);
}

void AutofillCacheResetter::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  std::move(callback_).Run(kCacheResetDone);
}

InternalsUIHandler::InternalsUIHandler(
    std::string call_on_load,
    base::Value call_on_load_argument,
    GetLogRouterFunction get_log_router_function)
    : call_on_load_(std::move(call_on_load)),
      call_on_load_argument_(std::move(call_on_load_argument)),
      get_log_router_function_(std::move(get_log_router_function)) {}

InternalsUIHandler::~InternalsUIHandler() {
  EndSubscription();
}

void InternalsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded", base::BindRepeating(&InternalsUIHandler::OnLoaded,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetCache", base::BindRepeating(&InternalsUIHandler::OnResetCache,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dumpAddresses", base::BindRepeating(&InternalsUIHandler::OnDumpAddresses,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAutofillAiCache",
      base::BindRepeating(&InternalsUIHandler::OnGetAutofillAiCache,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAutofillAiEntities",
      base::BindRepeating(&InternalsUIHandler::OnGetAutofillAiEntities,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "authenticateToRevealMaskedEntities",
      base::BindRepeating(
          &InternalsUIHandler::OnAuthenticateToRevealMaskedEntities,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeAutofillAiCacheEntry",
      base::BindRepeating(&InternalsUIHandler::OnDeleteAutofillAiCacheEntry,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "checkAtMemoryPermissions",
      base::BindRepeating(&InternalsUIHandler::CheckAtMemoryPermissions,
                          base::Unretained(this)));
#if !BUILDFLAG(IS_ANDROID)
  web_ui()->RegisterMessageCallback(
      "checkAutofillAiPermissions",
      base::BindRepeating(&InternalsUIHandler::CheckAutofillAiPermissions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDomNodeId", base::BindRepeating(&InternalsUIHandler::SetDomNodeId,
                                          base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "setPasswordChangeOverrideUrl",
      base::BindRepeating(&InternalsUIHandler::OnSetPasswordChangeOverrideUrl,
                          base::Unretained(this)));
}

void InternalsUIHandler::OnJavascriptAllowed() {
  StartSubscription();
}

void InternalsUIHandler::OnJavascriptDisallowed() {
  EndSubscription();
}

void InternalsUIHandler::OnDeleteAutofillAiCacheEntry(
    const base::ListValue& args) {
  AutofillAiModelCache* model_cache =
      AutofillAiModelCacheFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  uint64_t number;
  if (!model_cache || args.size() != 1 || !args[0].is_string() ||
      !base::StringToUint64(args[0].GetString(), &number)) {
    return;
  }
  model_cache->Erase(FormSignature(number));
}

void InternalsUIHandler::OnGetAutofillAiCache(const base::ListValue& args) {
  AutofillAiModelCache* model_cache =
      AutofillAiModelCacheFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (!model_cache) {
    FireWebUIListener("display-autofill-ai-cache", base::ListValue());
    return;
  }

  base::ListValue results;
  for (const auto& [form_signature, cache_entry] :
       model_cache->GetAllEntries()) {
    const int num_fields =
        std::min(cache_entry.field_identifiers_size(),
                 cache_entry.server_response().field_responses_size());
    auto fields = base::ListValue::with_capacity(num_fields);
    for (int i = 0; i < num_fields; ++i) {
      const auto& field_response =
          cache_entry.server_response().field_responses(i);
      const auto& field_identifier = cache_entry.field_identifiers(i);
      auto field_info =
          base::DictValue()
              .Set("signature",
                   base::NumberToString(field_identifier.field_signature()))
              .Set("rank",
                   base::NumberToString(
                       field_identifier.field_rank_in_signature_group()))
              .Set("type", FieldTypeToStringView(
                               ToSafeFieldType(field_response.field_type())
                                   .value_or(UNKNOWN_TYPE)));
      if (!field_response.formatting_meta().empty()) {
        field_info.Set("format", field_response.formatting_meta());
      }
      fields.Append(std::move(field_info));
    }
    results.Append(
        base::DictValue()
            .Set("formSignature", base::NumberToString(*form_signature))
            .Set("creationTime",
                 base::TimeFormatFriendlyDateAndTime(
                     base::Time::FromDeltaSinceWindowsEpoch(
                         base::Microseconds(cache_entry.creation_time()))))
            .Set("fields", std::move(fields)));
  }

  FireWebUIListener("display-autofill-ai-cache", std::move(results));
}

namespace {

// Returns a human-readable string representation of
// `EntityInstance::RecordType` such as `kLocal`, `kServerWallet`, or
// `kPersonalContext`.
std::string_view RecordTypeToStringView(
    EntityInstance::RecordType record_type) {
  switch (record_type) {
    case EntityInstance::RecordType::kLocal:
      return "Local";
    case EntityInstance::RecordType::kServerWallet:
      return "Server Wallet";
    case EntityInstance::RecordType::kPersonalContext:
      return "Personal Context";
  }
}

}  // namespace

void InternalsUIHandler::OnGetAutofillAiEntities(const base::ListValue& args) {
  EntityDataManager* entity_data_manager =
      AutofillEntityDataManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (entity_data_manager && !entity_data_observation_.IsObserving()) {
    entity_data_observation_.Observe(entity_data_manager);
  }

  AutofillAiPersonalContextAccessManager* pcam =
      AutofillAiPersonalContextAccessManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (pcam) {
    // OnGetAutofillAiEntities can be called repeatedly on tab clicks or
    // refreshes; avoid CHECK-failing if already observing.
    if (!pcontext_observation_.IsObserving()) {
      pcontext_observation_.Observe(pcam);
    }
    pending_prefetch_types_.clear();
    for (EntityType entity_type : DenseSet<EntityType>::all()) {
      pending_prefetch_types_.push_back(entity_type);
    }
    current_prefetch_type_ = std::nullopt;
    FetchNextPersonalContextType();
  }

  SendAutofillAiEntitiesToWebUI();
}

void InternalsUIHandler::OnEntityInstancesChanged() {
  SendAutofillAiEntitiesToWebUI();
}

// Fetches personal context entity types sequentially, one by one. Sequential
// queuing is required because PersonalContextManager::FetchContext limits
// parallel fetchers per feature to 2; dispatching all types simultaneously
// would cause newer requests to evict and cancel pending requests.
void InternalsUIHandler::FetchNextPersonalContextType() {
  if (pending_prefetch_types_.empty()) {
    current_prefetch_type_ = std::nullopt;
    FireWebUIListener("display-autofill-ai-loading-status", base::Value(""));
    return;
  }

  current_prefetch_type_ = pending_prefetch_types_.front();
  pending_prefetch_types_.pop_front();

  AutofillAiPersonalContextAccessManager* pcam =
      AutofillAiPersonalContextAccessManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (pcam &&
      pcam->GetPrefetchStatusByEntityType(*current_prefetch_type_) !=
          AutofillAiPersonalContextAccessManager::RequestStatus::kNotStarted) {
    current_prefetch_type_ = std::nullopt;
    FetchNextPersonalContextType();
    return;
  }

  std::string msg =
      base::StrCat({"Fetching ", current_prefetch_type_->name_as_string(),
                    " entities from CMS..."});
  FireWebUIListener("display-autofill-ai-loading-status", base::Value(msg));

  if (pcam) {
    pcam->PrefetchContext({*current_prefetch_type_});
  }
}

void InternalsUIHandler::OnPrefetchContextComplete(
    const AutofillAiPersonalContextAccessManager& manager,
    std::optional<base::span<const EntityInstance>> entities) {
  // Guard against spurious global observer broadcasts or intermediate presence
  // signal callbacks while our target entity type is still actively in flight.
  if (!current_prefetch_type_ ||
      manager.GetPrefetchStatusByEntityType(*current_prefetch_type_) ==
          AutofillAiPersonalContextAccessManager::RequestStatus::kPending) {
    SendAutofillAiEntitiesToWebUI();
    return;
  }
  current_prefetch_type_ = std::nullopt;
  FetchNextPersonalContextType();
  SendAutofillAiEntitiesToWebUI();
}

void InternalsUIHandler::SendAutofillAiEntitiesToWebUI() {
  EntityDataManager* entity_data_manager =
      AutofillEntityDataManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (!entity_data_manager) {
    FireWebUIListener("display-autofill-ai-entities", base::ListValue());
    return;
  }

  base::ListValue results;
  for (const EntityInstance& entity :
       entity_data_manager->GetEntityInstances()) {
    base::ListValue attributes_list;
    for (AttributeType attribute_type : entity.type().attributes()) {
      base::optional_ref<const AttributeInstance> attribute_instance =
          entity.attribute(attribute_type);
      std::string value;
      if (attribute_instance &&
          !attribute_instance->GetCompleteRawInfo().empty()) {
        value =
            attribute_type.is_obfuscated()
                ? "<redacted>"
                : base::UTF16ToUTF8(attribute_instance->GetCompleteRawInfo());
      }
      attributes_list.Append(base::DictValue()
                                 .Set("name", attribute_type.name_as_string())
                                 .Set("value", std::move(value)));
    }
    results.Append(
        base::DictValue()
            .Set("guid", entity.guid().value())
            .Set("nickname", entity.nickname())
            .Set("entityType", entity.type().name_as_string())
            .Set("recordType", RecordTypeToStringView(entity.record_type()))
            .Set("attributes", std::move(attributes_list)));
  }

  FireWebUIListener("display-autofill-ai-entities", std::move(results));
}

void InternalsUIHandler::OnAuthenticateToRevealMaskedEntities(
    const base::ListValue& args) {
  if (!authenticator_) {
    ContentAutofillClient* client =
        ContentAutofillClient::FromWebContents(web_ui()->GetWebContents());
    if (!client) {
      return;
    }
    authenticator_ = client->GetDeviceAuthenticator();
  }
  if (!authenticator_ ||
      !authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    OnReauthCompleted(/*auth_succeeded=*/true);
    return;
  }
  std::u16string message = u"Authenticate to view sensitive Autofill AI data.";
  authenticator_->AuthenticateWithMessage(
      message, base::BindOnce(&InternalsUIHandler::OnReauthCompleted,
                              weak_ptr_factory_.GetWeakPtr()));
}

void InternalsUIHandler::OnReauthCompleted(bool auth_succeeded) {
  authenticator_.reset();
  if (auth_succeeded) {
    SendAutofillAiEntitiesToWebUI();
  }
}

void InternalsUIHandler::OnLoaded(const base::ListValue& args) {
  AllowJavascript();
  FireWebUIListener(call_on_load_, call_on_load_argument_);
  // This is only available in contents, because the iOS BrowsingDataRemover
  // does not allow selectively deleting data per origin and we don't want to
  // wipe the entire cache.
  FireWebUIListener("enable-reset-cache-button", base::Value());
  FireWebUIListener(
      "notify-about-incognito",
      base::Value(Profile::FromWebUI(web_ui())->IsIncognitoProfile()));
  FireWebUIListener("notify-about-variations", version_ui::GetVariationsList());
}

void InternalsUIHandler::OnResetCache(const base::ListValue& args) {
  if (!autofill_cache_resetter_) {
    content::BrowserContext* browser_context = Profile::FromWebUI(web_ui());
    autofill_cache_resetter_.emplace(browser_context);
  }
  autofill_cache_resetter_->ResetCache(base::BindOnce(
      &InternalsUIHandler::OnResetCacheDone, base::Unretained(this)));
}

void InternalsUIHandler::OnResetCacheDone(const std::string& message) {
  FireWebUIListener("notify-reset-done", base::Value(message));
}

void InternalsUIHandler::OnDumpAddresses(const base::ListValue& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  PersonalDataManager* pdm =
      PersonalDataManagerFactory::GetForBrowserContext(profile);
  if (!pdm) {
    return;
  }
  LogBuffer log;
  for (const AutofillProfile* address :
       pdm->address_data_manager().GetProfiles()) {
    log << *address;
  }
  if (std::optional<base::DictValue> result = log.RetrieveResult()) {
    LogEntry(*result);
  }
}

void InternalsUIHandler::CheckAtMemoryPermissions(const base::ListValue& args) {
  if (args.size() < 1 || !args[0].is_string()) {
    return;
  }
  std::optional<AtMemoryAction> action;
  const std::string& action_str = args[0].GetString();
  // LINT.IfChange(AtMemoryAction)
  if (action_str == "kTriggerSearchUI") {
    action = AtMemoryAction::kTriggerSearchUI;
  } else if (action_str == "kShowAtMemoryInSettings") {
    action = AtMemoryAction::kShowAtMemoryInSettings;
  } else if (action_str == "kAllowCustomizeAtMemoryShortcut") {
    action = AtMemoryAction::kAllowCustomizeAtMemoryShortcut;
  } else if (action_str == "kShowIph") {
    action = AtMemoryAction::kShowIph;
  } else if (action_str == "kShowAutocompleteAtMemoryButton") {
    action = AtMemoryAction::kShowAutocompleteAtMemoryButton;
  } else if (action_str == "kRetrievePaymentsForFilling") {
    action = AtMemoryAction::kRetrievePaymentsForFilling;
  } else if (action_str == "kRetrieveContactInfoForFilling") {
    action = AtMemoryAction::kRetrieveContactInfoForFilling;
  } else if (action_str == "kRetrieveIdentityDocsForFilling") {
    action = AtMemoryAction::kRetrieveIdentityDocsForFilling;
  } else if (action_str == "kRetrieveTravelDataForFilling") {
    action = AtMemoryAction::kRetrieveTravelDataForFilling;
  } else if (action_str == "kRetrieveShoppingDataForFilling") {
    action = AtMemoryAction::kRetrieveShoppingDataForFilling;
  }
  // LINT.ThenChange(/components/autofill/core/browser/at_memory/at_memory_enablement_utils.h:AtMemoryAction)
  if (!action.has_value()) {
    return;
  }

  std::optional<GURL> url;
  if (args.size() >= 2 && args[1].is_string() && !args[1].GetString().empty()) {
    GURL parsed_url(args[1].GetString());
    if (parsed_url.is_valid()) {
      url = std::move(parsed_url);
    }
  }

  ContentAutofillClient& client = CHECK_DEREF(
      ContentAutofillClient::FromWebContents(web_ui()->GetWebContents()));
  std::string debug_message;
  const auto sources =
      std::to_array({MemoryEntrySource{MemoryEntrySourceType::kAutofill}});
  std::optional<RetrieveForFillingParams> retrieve_params;
  if (IsRetrieveForFillingAction(*action)) {
    retrieve_params =
        RetrieveForFillingParams{.is_spii = false,
                                 .sources = sources,
                                 .is_context_secure = client.IsContextSecure()};
  }

  const bool may_perform = MayPerformAtMemoryAction(
      *action, client, url, retrieve_params, &debug_message);
  FireWebUIListener(
      "on-at-memory-permission-check-done",
      base::Value(may_perform
                      ? "AtMemory action is allowed"
                      : base::StrCat({"AtMemory action is not allowed: ",
                                      debug_message})));
}

#if !BUILDFLAG(IS_ANDROID)
void InternalsUIHandler::CheckAutofillAiPermissions(
    const base::ListValue& args) {
  std::string debug_message;
  const bool may_opt_in = MayPerformAutofillAiAction(
      CHECK_DEREF(
          ContentAutofillClient::FromWebContents(web_ui()->GetWebContents())),
      AutofillAiAction::kOptIn, /*entity_type=*/std::nullopt, &debug_message);
  FireWebUIListener(
      "on-autofill-ai-permission-check-done",
      base::Value(
          may_opt_in ? "Autofill with AI opt-in is allowed"
                     : base::StrCat({"Autofill with AI opt-in is not allowed: ",
                                     debug_message})));
}

void InternalsUIHandler::SetDomNodeId(const base::ListValue& args) {
  for (auto* browser : GetAllBrowserWindowInterfaces()) {
    if (!browser->GetTabStripModel()) {
      continue;
    }

    for (int i = 0; i < browser->GetTabStripModel()->count(); i++) {
      auto* web_contents = browser->GetTabStripModel()->GetWebContentsAt(i);
      AutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
          web_contents->GetPrimaryMainFrame());
      if (driver) {
        driver->ExposeDomNodeIdsInAllFrames();
      }
    }
  }
}
#endif

void InternalsUIHandler::StartSubscription() {
  LogRouter* log_router =
      get_log_router_function_.Run(Profile::FromWebUI(web_ui()));
  if (!log_router) {
    return;
  }

  registered_with_log_router_ = true;
  log_router->RegisterReceiver(this);
}

void InternalsUIHandler::EndSubscription() {
  if (!registered_with_log_router_) {
    return;
  }
  registered_with_log_router_ = false;
  LogRouter* log_router =
      get_log_router_function_.Run(Profile::FromWebUI(web_ui()));
  if (log_router) {
    log_router->UnregisterReceiver(this);
  }
}

void InternalsUIHandler::OnSetPasswordChangeOverrideUrl(
    const base::ListValue& args) {
  if (args.size() != 1 || !args[0].is_string()) {
    return;
  }
  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_ui()->GetWebContents());
  if (factory) {
    password_manager::PasswordManagerClient* client =
        factory->password_client();
    if (client) {
      password_manager::PasswordChangeServiceInterface* service =
          client->GetPasswordChangeService();
      if (service) {
        service->AddChangePasswordUrlOverride(GURL(args[0].GetString()));
      }
    }
  }
}

void InternalsUIHandler::LogEntry(const base::DictValue& entry) {
  if (!registered_with_log_router_) {
    return;
  }
  FireWebUIListener("add-structured-log", entry);
}

}  // namespace autofill
