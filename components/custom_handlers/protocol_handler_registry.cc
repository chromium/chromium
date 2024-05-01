// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/protocol_handler_registry.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/custom_handlers/pref_names.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/child_process_security_policy.h"
#include "url/url_util.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

namespace custom_handlers {

namespace {

const ProtocolHandler& LookupHandler(
    const ProtocolHandlerRegistry::ProtocolHandlerMap& handler_map,
    std::string_view scheme) {
  auto p = handler_map.find(scheme);

  if (p != handler_map.end())
    return p->second;

  return ProtocolHandler::EmptyProtocolHandler();
}

GURL TranslateUrl(
    const ProtocolHandlerRegistry::ProtocolHandlerMap& handler_map,
    const GURL& url) {
  const ProtocolHandler& handler =
      LookupHandler(handler_map, url.scheme_piece());
  if (handler.IsEmpty())
    return GURL();

  GURL translated_url(handler.TranslateUrl(url));
  if (!translated_url.is_valid())
    return GURL();

  return translated_url;
}

}  // namespace

// ProtocolHandlerRegistry -----------------------------------------------------

std::unique_ptr<ProtocolHandlerRegistry> ProtocolHandlerRegistry::Create(
    PrefService* prefs,
    std::unique_ptr<Delegate> delegate) {
  auto registry =
      std::make_unique<ProtocolHandlerRegistry>(prefs, std::move(delegate));

  // If installing defaults, they must be installed prior calling
  // InitProtocolSettings.
  registry->InstallPredefinedHandlers();

  registry->InitProtocolSettings();

  return registry;
}

ProtocolHandlerRegistry::ProtocolHandlerRegistry(
    PrefService* prefs,
    std::unique_ptr<Delegate> delegate)
    : prefs_(prefs),
      delegate_(std::move(delegate)),
      enabled_(true),
      is_loading_(false),
      is_loaded_(false) {}

bool ProtocolHandlerRegistry::SilentlyHandleRegisterHandlerRequest(
    const ProtocolHandler& handler) {
  if (handler.IsEmpty() || !CanSchemeBeOverridden(handler.protocol()))
    return true;

  if (!enabled() || IsRegistered(handler) || HasIgnoredEquivalent(handler))
    return true;

  if (AttemptReplace(handler))
    return true;

  return false;
}

void ProtocolHandlerRegistry::OnAcceptRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RegisterProtocolHandler(handler, USER))
    return;
  SetDefault(handler);
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::OnDenyRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RegisterProtocolHandler(handler, USER);
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::OnIgnoreRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  IgnoreProtocolHandler(handler, USER);
  Save();
  NotifyChanged();
}

bool ProtocolHandlerRegistry::AttemptReplace(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandler old_default = GetHandlerFor(handler.protocol());
  bool make_new_handler_default = handler.IsSameOrigin(old_default);
  ProtocolHandlerList to_replace(GetReplacedHandlers(handler));
  if (to_replace.empty())
    return false;
  for (const auto& replaced_handler : to_replace) {
    RemoveHandler(replaced_handler);
  }
  if (make_new_handler_default) {
    OnAcceptRegisterProtocolHandler(handler);
  } else {
    InsertHandler(handler);
    NotifyChanged();
  }
  return true;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetReplacedHandlers(
    const ProtocolHandler& handler) const {
  ProtocolHandlerList replaced_handlers;
  const ProtocolHandlerList* handlers = GetHandlerList(handler.protocol());
  if (!handlers)
    return replaced_handlers;
  for (const auto& old_handler : *handlers) {
    if (handler.IsSameOrigin(old_handler)) {
      replaced_handlers.push_back(old_handler);
    }
  }
  return replaced_handlers;
}

void ProtocolHandlerRegistry::ClearDefault(const std::string& scheme) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(jfernandez): If we want to use std::string_view as map's key for
  // erasing, we would need to adapt the ProtocolHandlerMap, or just use the
  // iterator got from find(scheme).
  default_handlers_.erase(scheme);
  Save();
  NotifyChanged();
}

bool ProtocolHandlerRegistry::IsDefault(const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetHandlerFor(handler.protocol()) == handler;
}

void ProtocolHandlerRegistry::InstallPredefinedHandlers() {
  for (const auto& [scheme, handler] : url::GetPredefinedHandlerSchemes()) {
    AddPredefinedHandler(
        ProtocolHandler::CreateProtocolHandler(scheme, GURL(handler)));
  }
}

void ProtocolHandlerRegistry::InitProtocolSettings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Any further default additions to the table will get rejected from now on.
  is_loaded_ = true;
  is_loading_ = true;

  if (!prefs_) {
    is_loading_ = false;
    return;
  }

  if (prefs_->HasPrefPath(prefs::kCustomHandlersEnabled)) {
    if (prefs_->GetBoolean(prefs::kCustomHandlersEnabled)) {
      Enable();
    } else {
      Disable();
    }
  }

  RegisterProtocolHandlersFromPref(prefs::kPolicyRegisteredProtocolHandlers,
                                   POLICY);
  RegisterProtocolHandlersFromPref(prefs::kRegisteredProtocolHandlers, USER);
  IgnoreProtocolHandlersFromPref(prefs::kPolicyIgnoredProtocolHandlers, POLICY);
  IgnoreProtocolHandlersFromPref(prefs::kIgnoredProtocolHandlers, USER);

  is_loading_ = false;

  // For each default protocol handler, check that we are still registered
  // with the OS as the default application.
  if (delegate_->ShouldRemoveHandlersNotInOS()) {
    for (const auto& [protocol, handler] : default_handlers_) {
      delegate_->CheckDefaultClientWithOS(
          protocol, GetDefaultWebClientCallback(protocol));
    }
  }
}

int ProtocolHandlerRegistry::GetHandlerIndex(std::string_view scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandler& candidate = GetHandlerFor(scheme);
  if (candidate.IsEmpty())
    return -1;
  const ProtocolHandlerList* handlers = GetHandlerList(scheme);
  if (!handlers)
    return -1;

  int i = 0;
  for (const auto& handler : *handlers) {
    if (handler == candidate)
      return i;
    i++;
  }
  return -1;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetHandlersFor(std::string_view scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto p = protocol_handlers_.find(scheme);
  if (p == protocol_handlers_.end()) {
    return ProtocolHandlerList();
  }
  return p->second;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetUserDefinedHandlers(base::Time begin,
                                                base::Time end) const {
  ProtocolHandlerRegistry::ProtocolHandlerList result;
  for (const auto& [protocol, handlers_list] : user_protocol_handlers_) {
    for (const ProtocolHandler& handler : handlers_list) {
      if (base::Contains(predefined_protocol_handlers_, handler))
        continue;
      if (begin <= handler.last_modified() && handler.last_modified() < end)
        result.push_back(handler);
    }
  }
  return result;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetUserIgnoredHandlers(base::Time begin,
                                                base::Time end) const {
  ProtocolHandlerRegistry::ProtocolHandlerList result;
  for (const ProtocolHandler& handler : user_ignored_protocol_handlers_) {
    if (begin <= handler.last_modified() && handler.last_modified() < end)
      result.push_back(handler);
  }
  return result;
}

void ProtocolHandlerRegistry::ClearUserDefinedHandlers(base::Time begin,
                                                       base::Time end) {
  for (const ProtocolHandler& handler : GetUserDefinedHandlers(begin, end))
    RemoveHandler(handler);

  for (const ProtocolHandler& handler : GetUserIgnoredHandlers(begin, end))
    RemoveIgnoredHandler(handler);
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetIgnoredHandlers() {
  return ignored_protocol_handlers_;
}

void ProtocolHandlerRegistry::GetRegisteredProtocols(
    std::vector<std::string>* output) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& [protocol, handlers_list] : protocol_handlers_) {
    if (!handlers_list.empty())
      output->push_back(protocol);
  }
}

bool ProtocolHandlerRegistry::CanSchemeBeOverridden(
    std::string_view scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandlerList* handlers = GetHandlerList(scheme);
  // If we already have a handler for this scheme, we can add more.
  if (handlers != NULL && !handlers->empty())
    return true;
  // Don't override a scheme if it already has an external handler.
  return !delegate_->IsExternalHandlerRegistered(
      static_cast<std::string>(scheme));
}

bool ProtocolHandlerRegistry::IsRegistered(
    const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandlerList* handlers = GetHandlerList(handler.protocol());
  if (!handlers) {
    return false;
  }
  return base::Contains(*handlers, handler);
}

bool ProtocolHandlerRegistry::IsRegisteredByUser(
    const ProtocolHandler& handler) {
  return HandlerExists(handler, &user_protocol_handlers_);
}

bool ProtocolHandlerRegistry::HasPolicyRegisteredHandler(
    std::string_view scheme) {
  return (policy_protocol_handlers_.find(scheme) !=
          policy_protocol_handlers_.end());
}

bool ProtocolHandlerRegistry::IsIgnored(const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& ignored_handler : ignored_protocol_handlers_) {
    if (ignored_handler == handler) {
      return true;
    }
  }
  return false;
}

bool ProtocolHandlerRegistry::HasRegisteredEquivalent(
    const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandlerList* handlers = GetHandlerList(handler.protocol());
  if (!handlers) {
    return false;
  }
  for (const auto& registered_handler : *handlers) {
    if (handler.IsEquivalent(registered_handler)) {
      return true;
    }
  }
  return false;
}

bool ProtocolHandlerRegistry::HasIgnoredEquivalent(
    const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& ignored_handler : ignored_protocol_handlers_) {
    if (handler.IsEquivalent(ignored_handler)) {
      return true;
    }
  }
  return false;
}

void ProtocolHandlerRegistry::RemoveIgnoredHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool should_notify = false;
  if (HandlerExists(handler, ignored_protocol_handlers_) &&
      HandlerExists(handler, user_ignored_protocol_handlers_)) {
    EraseHandler(handler, &user_ignored_protocol_handlers_);
    Save();
    if (!HandlerExists(handler, policy_ignored_protocol_handlers_)) {
      EraseHandler(handler, &ignored_protocol_handlers_);
      should_notify = true;
    }
  }
  if (should_notify)
    NotifyChanged();
}

bool ProtocolHandlerRegistry::IsHandledProtocol(std::string_view scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return enabled_ && !GetHandlerFor(scheme).IsEmpty();
}

void ProtocolHandlerRegistry::RemoveHandler(const ProtocolHandler& handler) {
  if (IsIgnored(handler)) {
    RemoveIgnoredHandler(handler);
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandlerList& handlers = protocol_handlers_[handler.protocol()];
  bool erase_success = false;
  if (HandlerExists(handler, handlers) &&
      HandlerExists(handler, &user_protocol_handlers_)) {
    EraseHandler(handler, &user_protocol_handlers_);
    erase_success = true;
    if (!HandlerExists(handler, &policy_protocol_handlers_))
      EraseHandler(handler, &protocol_handlers_);
  }
  auto q = default_handlers_.find(handler.protocol());
  if (erase_success && q != default_handlers_.end() && q->second == handler) {
    // Make the new top handler in the list the default.
    if (!handlers.empty()) {
      // NOTE We pass a copy because SetDefault() modifies handlers.
      SetDefault(ProtocolHandler(handlers[0]));
    } else {
      default_handlers_.erase(q);
    }
  }

  if (erase_success && !IsHandledProtocol(handler.protocol())) {
    delegate_->DeregisterExternalHandler(handler.protocol());
  }
  Save();
  if (erase_success)
    NotifyChanged();
}

void ProtocolHandlerRegistry::RemoveDefaultHandler(std::string_view scheme) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandler current_default = GetHandlerFor(scheme);
  if (!current_default.IsEmpty())
    RemoveHandler(current_default);
}

const ProtocolHandler& ProtocolHandlerRegistry::GetHandlerFor(
    std::string_view scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return LookupHandler(default_handlers_, scheme);
}

GURL ProtocolHandlerRegistry::Translate(const GURL& url) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return TranslateUrl(default_handlers_, url);
}

void ProtocolHandlerRegistry::Enable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (enabled_) {
    return;
  }
  enabled_ = true;
  for (const auto& [protocol, handler] : default_handlers_) {
    delegate_->RegisterExternalHandler(protocol);
  }
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::Disable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!enabled_) {
    return;
  }
  enabled_ = false;

  for (const auto& [protocol, handler] : default_handlers_) {
    delegate_->DeregisterExternalHandler(protocol);
  }
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_.reset(NULL);

  weak_ptr_factory_.InvalidateWeakPtrs();
}

// static
void ProtocolHandlerRegistry::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kRegisteredProtocolHandlers);
  registry->RegisterListPref(prefs::kIgnoredProtocolHandlers);
  registry->RegisterListPref(prefs::kPolicyRegisteredProtocolHandlers);
  registry->RegisterListPref(prefs::kPolicyIgnoredProtocolHandlers);
  registry->RegisterBooleanPref(prefs::kCustomHandlersEnabled, true);
}

ProtocolHandlerRegistry::~ProtocolHandlerRegistry() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ProtocolHandlerRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProtocolHandlerRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProtocolHandlerRegistry::PromoteHandler(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsRegistered(handler));
  auto p = protocol_handlers_.find(handler.protocol());
  ProtocolHandlerList& list = p->second;
  list.erase(base::ranges::find(list, handler));
  list.insert(list.begin(), handler);
}

void ProtocolHandlerRegistry::Save() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_loading_) {
    return;
  }

  if (!prefs_)
    return;

  base::Value registered_protocol_handlers(EncodeRegisteredHandlers());
  base::Value ignored_protocol_handlers(EncodeIgnoredHandlers());

  prefs_->Set(prefs::kRegisteredProtocolHandlers, registered_protocol_handlers);
  prefs_->Set(prefs::kIgnoredProtocolHandlers, ignored_protocol_handlers);
  prefs_->SetBoolean(prefs::kCustomHandlersEnabled, enabled_);
}

const ProtocolHandlerRegistry::ProtocolHandlerList*
ProtocolHandlerRegistry::GetHandlerList(std::string_view scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto p = protocol_handlers_.find(scheme);
  if (p == protocol_handlers_.end()) {
    return NULL;
  }
  return &p->second;
}

void ProtocolHandlerRegistry::SetDefault(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::string& protocol = handler.protocol();
  ProtocolHandlerMap::const_iterator p = default_handlers_.find(protocol);
  // If we're not loading, and we are setting a default for a new protocol,
  // register with the OS.
  if (!is_loading_ && p == default_handlers_.end())
    delegate_->RegisterWithOSAsDefaultClient(
        protocol, GetDefaultWebClientCallback(protocol));
  default_handlers_.erase(protocol);
  default_handlers_.insert(std::make_pair(protocol, handler));

  PromoteHandler(handler);
}

void ProtocolHandlerRegistry::InsertHandler(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto p = protocol_handlers_.find(handler.protocol());

  if (p != protocol_handlers_.end()) {
    p->second.push_back(handler);
    return;
  }

  ProtocolHandlerList new_list;
  new_list.push_back(handler);
  protocol_handlers_[handler.protocol()] = new_list;
}

base::Value::List ProtocolHandlerRegistry::EncodeRegisteredHandlers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value::List encoded_handlers;
  for (const auto& [protocol, handlers_list] : user_protocol_handlers_) {
    for (const auto& handler : handlers_list) {
      base::Value::Dict encoded = handler.Encode();
      if (IsDefault(handler)) {
        encoded.Set("default", true);
      }
      encoded_handlers.Append(std::move(encoded));
    }
  }
  return encoded_handlers;
}

base::Value::List ProtocolHandlerRegistry::EncodeIgnoredHandlers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value::List encoded_handlers;
  for (const auto& handler : user_ignored_protocol_handlers_) {
    encoded_handlers.Append(handler.Encode());
  }
  return encoded_handlers;
}

void ProtocolHandlerRegistry::NotifyChanged() {
  for (auto& observer : observers_)
    observer.OnProtocolHandlerRegistryChanged();
}

bool ProtocolHandlerRegistry::RegisterProtocolHandler(
    const ProtocolHandler& handler,
    const HandlerSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(CanSchemeBeOverridden(handler.protocol()));
  DCHECK(!handler.IsEmpty());

  // Ignore invalid handlers.
  if (!handler.IsValid())
    return false;

  ProtocolHandlerMultiMap& map =
      (source == POLICY) ? policy_protocol_handlers_ : user_protocol_handlers_;
  ProtocolHandlerList& list = map[handler.protocol()];
  if (!HandlerExists(handler, list))
    list.push_back(handler);
  if (IsRegistered(handler)) {
    return true;
  }
  if (enabled_ && !delegate_->IsExternalHandlerRegistered(handler.protocol()))
    delegate_->RegisterExternalHandler(handler.protocol());
  InsertHandler(handler);
  return true;
}

std::vector<const base::Value::Dict*>
ProtocolHandlerRegistry::GetHandlersFromPref(const char* pref_name) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<const base::Value::Dict*> result;
  if (!prefs_ || !prefs_->HasPrefPath(pref_name)) {
    return result;
  }

  const base::Value::List& handlers = prefs_->GetList(pref_name);

  for (const auto& list_item : handlers) {
    if (const base::Value::Dict* encoded_handler = list_item.GetIfDict()) {
      if (ProtocolHandler::IsValidDict(*encoded_handler)) {
        result.push_back(encoded_handler);
      }
    }
  }

  return result;
}

void ProtocolHandlerRegistry::RegisterProtocolHandlersFromPref(
    const char* pref_name,
    const HandlerSource source) {
  std::vector<const base::Value::Dict*> registered_handlers =
      GetHandlersFromPref(pref_name);
  for (const auto* encoded_handler : registered_handlers) {
    ProtocolHandler handler =
        ProtocolHandler::CreateProtocolHandler(*encoded_handler);
    if (!RegisterProtocolHandler(handler, source))
      continue;
    if (encoded_handler->FindBool("default").value_or(false)) {
      SetDefault(handler);
    }
  }
}

void ProtocolHandlerRegistry::IgnoreProtocolHandler(
    const ProtocolHandler& handler,
    const HandlerSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandlerList& list = (source == POLICY)
                                  ? policy_ignored_protocol_handlers_
                                  : user_ignored_protocol_handlers_;
  if (!HandlerExists(handler, list))
    list.push_back(handler);
  if (HandlerExists(handler, ignored_protocol_handlers_))
    return;
  ignored_protocol_handlers_.push_back(handler);
}

void ProtocolHandlerRegistry::IgnoreProtocolHandlersFromPref(
    const char* pref_name,
    const HandlerSource source) {
  std::vector<const base::Value::Dict*> ignored_handlers =
      GetHandlersFromPref(pref_name);
  for (const auto* encoded_handler : ignored_handlers)
    IgnoreProtocolHandler(
        ProtocolHandler::CreateProtocolHandler(*encoded_handler), source);
}

bool ProtocolHandlerRegistry::HandlerExists(const ProtocolHandler& handler,
                                            ProtocolHandlerMultiMap* map) {
  return HandlerExists(handler, (*map)[handler.protocol()]);
}

bool ProtocolHandlerRegistry::HandlerExists(const ProtocolHandler& handler,
                                            const ProtocolHandlerList& list) {
  return base::Contains(list, handler);
}

void ProtocolHandlerRegistry::EraseHandler(const ProtocolHandler& handler,
                                           ProtocolHandlerMultiMap* map) {
  EraseHandler(handler, &(*map)[handler.protocol()]);
}

void ProtocolHandlerRegistry::EraseHandler(const ProtocolHandler& handler,
                                           ProtocolHandlerList* list) {
  list->erase(base::ranges::find(*list, handler));
}

void ProtocolHandlerRegistry::OnSetAsDefaultProtocolClientFinished(
    const std::string& protocol,
    bool is_default) {
  // Clear if the default protocol client isn't this installation.
  if (!is_default && delegate_->ShouldRemoveHandlersNotInOS())
    ClearDefault(protocol);
}

void ProtocolHandlerRegistry::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
}

base::WeakPtr<ProtocolHandlerRegistry> ProtocolHandlerRegistry::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProtocolHandlerRegistry::AddPredefinedHandler(
    const ProtocolHandler& handler) {
  DCHECK(!is_loaded_);  // Must be called prior InitProtocolSettings.
  RegisterProtocolHandler(handler, USER);
  SetDefault(handler);
  predefined_protocol_handlers_.push_back(handler);
}

DefaultClientCallback ProtocolHandlerRegistry::GetDefaultWebClientCallback(
    const std::string& protocol) {
  return base::BindOnce(
      &ProtocolHandlerRegistry::OnSetAsDefaultProtocolClientFinished,
      weak_ptr_factory_.GetWeakPtr(), protocol);
}

}  // namespace custom_handlers
