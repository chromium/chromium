// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/browser/js_communication_host.h"

#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "components/js_injection/browser/js_to_browser_messaging.h"
#include "components/js_injection/browser/navigation_web_message_sender.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_host_factory.h"
#include "components/js_injection/common/enum.mojom.h"
#include "components/origin_matcher/origin_matcher.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace js_injection {
namespace {

std::string ConvertToNativeAllowedOriginRulesWithSanityCheck(
    const std::vector<std::string>& allowed_origin_rules_strings,
    origin_matcher::OriginMatcher& allowed_origin_rules) {
  for (auto& rule : allowed_origin_rules_strings) {
    if (!allowed_origin_rules.AddRuleFromString(rule))
      return "allowedOriginRules " + rule + " is invalid";
  }
  return std::string();
}

// Performs ForEachRenderFrameHost starting from `render_frame_host`, but skips
// any inner WebContents.
void ForEachRenderFrameHostWithinSameWebContents(
    content::RenderFrameHost* render_frame_host,
    base::FunctionRef<void(content::RenderFrameHost*)> func_ref) {
  render_frame_host->ForEachRenderFrameHostWithAction(
      [starting_web_contents =
           content::WebContents::FromRenderFrameHost(render_frame_host),
       func_ref](content::RenderFrameHost* rfh) {
        // Don't cross into inner WebContents since we wouldn't be notified of
        // its changes.
        if (content::WebContents::FromRenderFrameHost(rfh) !=
            starting_web_contents) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        func_ref(rfh);
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
}

}  // namespace

struct JsObject {
  JsObject(const std::u16string& name,
           origin_matcher::OriginMatcher allowed_origin_rules,
           int32_t world_id,
           std::unique_ptr<WebMessageHostFactory> factory)
      : name(std::move(name)),
        allowed_origin_rules(std::move(allowed_origin_rules)),
        world_id(world_id),
        factory(std::move(factory)) {}
  JsObject(JsObject&& other) = delete;
  JsObject& operator=(JsObject&& other) = delete;
  ~JsObject() = default;

  std::u16string name;
  origin_matcher::OriginMatcher allowed_origin_rules;
  int32_t world_id;
  std::unique_ptr<WebMessageHostFactory> factory;
};

JavaScriptExecutable::JavaScriptExecutable(
    std::u16string script,
    mojom::DocumentInjectionTime event_type,
    origin_matcher::OriginMatcher allowed_origin_rules,
    int32_t world_identifier,
    int32_t script_id)
    : script_(std::move(script)),
      allowed_origin_rules_(allowed_origin_rules),
      script_id_(script_id),
      event_type_(event_type),
      world_identifier_(world_identifier) {}

JsCommunicationHost::AddScriptResult::AddScriptResult() = default;
JsCommunicationHost::AddScriptResult::AddScriptResult(
    const JsCommunicationHost::AddScriptResult&) = default;
JsCommunicationHost::AddScriptResult&
JsCommunicationHost::AddScriptResult::operator=(
    const JsCommunicationHost::AddScriptResult&) = default;
JsCommunicationHost::AddScriptResult::~AddScriptResult() = default;

// Holds a set of JsToBrowserMessaging objects for a frame and allows notifying
// the objects of renderer side messages.
class JsCommunicationHost::JsToBrowserMessagingList
    : public mojom::JsObjectsClient {
 public:
  JsToBrowserMessagingList(
      std::map<std::pair<std::u16string, int32_t>,
               std::unique_ptr<JsToBrowserMessaging>> js_to_browser_messagings,
      mojo::PendingAssociatedReceiver<mojom::JsObjectsClient> receiver)
      : js_to_browser_messagings_(std::move(js_to_browser_messagings)),
        receiver_(this, std::move(receiver)) {}

  // mojom::JsObjectsClient:
  void OnWindowObjectCleared() override {
    for (auto& kv : js_to_browser_messagings_) {
      // Send an empty remote here. The remote will be bound lazily when needed.
      kv.second->SetBrowserToJsMessaging({});
    }
  }

  const std::map<std::pair<std::u16string, int32_t>,
                 std::unique_ptr<JsToBrowserMessaging>>&
  js_to_browser_messagings() const {
    return js_to_browser_messagings_;
  }

 private:
  const std::map<std::pair<std::u16string, int32_t>,
                 std::unique_ptr<JsToBrowserMessaging>>
      js_to_browser_messagings_;
  mojo::AssociatedReceiver<mojom::JsObjectsClient> receiver_;
};

JsCommunicationHost::JsCommunicationHost(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

JsCommunicationHost::~JsCommunicationHost() = default;

JsCommunicationHost::AddScriptResult
JsCommunicationHost::AddPersistentJavaScript(
    std::u16string script,
    mojom::DocumentInjectionTime injection_time,
    const std::vector<std::string>& allowed_origin_rules,
    int world_identifier) {
  origin_matcher::OriginMatcher origin_matcher;
  std::string error_message = ConvertToNativeAllowedOriginRulesWithSanityCheck(
      allowed_origin_rules, origin_matcher);
  AddScriptResult result;
  if (!error_message.empty()) {
    result.error_message = std::move(error_message);
    return result;
  }

  sticky_scripts_.emplace_back(script, injection_time, origin_matcher,
                               world_identifier, next_script_id_++);

  ForEachRenderFrameHostWithinSameWebContents(
      web_contents()->GetPrimaryMainFrame(),
      [this](content::RenderFrameHost* render_frame_host) {
        NotifyFrameForPersistentJavaScript(&*sticky_scripts_.rbegin(),
                                           render_frame_host);
      });
  result.script_id = sticky_scripts_.rbegin()->script_id_;
  return result;
}

bool JsCommunicationHost::RemovePersistentJavaScript(int script_id) {
  for (auto it = sticky_scripts_.begin(); it != sticky_scripts_.end(); ++it) {
    if (it->script_id_ == script_id) {
      sticky_scripts_.erase(it);
      ForEachRenderFrameHostWithinSameWebContents(
          web_contents()->GetPrimaryMainFrame(),
          [this, script_id](content::RenderFrameHost* render_frame_host) {
            NotifyFrameForRemovePersistentJavaScript(script_id,
                                                     render_frame_host);
          });
      return true;
    }
  }
  return false;
}

const std::vector<JavaScriptExecutable>&
JsCommunicationHost::GetPersistentJavaScripts() const {
  return sticky_scripts_;
}

std::u16string JsCommunicationHost::AddWebMessageHostFactory(
    std::unique_ptr<WebMessageHostFactory> factory,
    const std::u16string& js_object_name,
    const std::vector<std::string>& allowed_origin_rules,
    int32_t world_identifier) {
  origin_matcher::OriginMatcher origin_matcher;
  std::string error_message = ConvertToNativeAllowedOriginRulesWithSanityCheck(
      allowed_origin_rules, origin_matcher);
  if (!error_message.empty())
    return base::UTF8ToUTF16(error_message);

  for (const auto& js_object : js_objects_) {
    if (js_object->name == js_object_name &&
        js_object->world_id == world_identifier) {
      return u"jsObjectName " + js_object->name +
             u" was already added for world.";
    }
  }

  if (NavigationWebMessageSender::IsNavigationListener(js_object_name)) {
    // This is the special navigationListener object that is registered to
    // listen for navigation events instead of establishing a connection to
    // the renderer. This shouldn't create an object in the renderer. Instead,
    // create a NavigationWebMessageSender for the primary Page, so that
    // navigation notifications for it will be sent.
    // TODO(https://crbug.com/332809183): Guard this behind an origin trial
    // check later on.
    has_navigation_listener_ = true;
    NavigationWebMessageSender::CreateForPageIfNeeded(
        web_contents()->GetPrimaryPage(), js_object_name, factory.get());
    NavigationWebMessageSender::GetForPage(web_contents()->GetPrimaryPage())
        ->DispatchOptInMessage();
  }

  js_objects_.push_back(std::make_unique<JsObject>(
      js_object_name, origin_matcher, world_identifier, std::move(factory)));

  // If a new message listener is added when a page is in BFCache or
  // prerendered, the listener won't be available when be page is activated
  // since it is not injected for the page. To avoid this behavior difference
  // when these features are involved vs not, evict all BFCached and prerendered
  // pages so that we won't activate any pages that don't have this object
  // injected.
  web_contents()->GetController().GetBackForwardCache().Flush(
      content::BackForwardCache::NotRestoredReason::
          kWebViewMessageListenerInjected);
  web_contents()->CancelAllPrerendering();

  ForEachRenderFrameHostWithinSameWebContents(
      web_contents()->GetPrimaryMainFrame(),
      [this](content::RenderFrameHost* render_frame_host) {
        NotifyFrameForWebMessageListener(render_frame_host);
      });
  return std::u16string();
}

void JsCommunicationHost::RemoveWebMessageHostFactory(
    const std::u16string& js_object_name,
    int32_t world_identifier) {
  for (auto iterator = js_objects_.begin(); iterator != js_objects_.end();
       ++iterator) {
    if ((*iterator)->name == js_object_name &&
        (*iterator)->world_id == world_identifier) {
      js_objects_.erase(iterator);
      ForEachRenderFrameHostWithinSameWebContents(
          web_contents()->GetPrimaryMainFrame(),
          [this](content::RenderFrameHost* render_frame_host) {
            NotifyFrameForWebMessageListener(render_frame_host);
          });
      break;
    }
  }
}

std::vector<JsCommunicationHost::RegisteredFactory>
JsCommunicationHost::GetWebMessageHostFactories() {
  const size_t num_objects = js_objects_.size();
  std::vector<RegisteredFactory> factories(num_objects);
  for (size_t i = 0; i < num_objects; ++i) {
    factories[i].js_name = js_objects_[i]->name;
    factories[i].allowed_origin_rules = js_objects_[i]->allowed_origin_rules;
    factories[i].factory = js_objects_[i]->factory.get();
    factories[i].world_id = js_objects_[i]->world_id;
  }
  return factories;
}

void JsCommunicationHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  base::ElapsedTimer timer;
  NotifyFrameForWebMessageListener(render_frame_host);
  NotifyFrameForAllPersistentJavaScripts(render_frame_host);
  base::UmaHistogramTimes("Android.WebView.JsInjection.RenderFrameCreatedTime",
                          timer.Elapsed());
}

void JsCommunicationHost::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  js_to_browser_messagings_.erase(render_frame_host->GetGlobalId());
}

void JsCommunicationHost::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  auto iter = js_to_browser_messagings_.find(render_frame_host->GetGlobalId());
  if (iter == js_to_browser_messagings_.end()) {
    return;
  }

  using LifecycleState = content::RenderFrameHost::LifecycleState;
  if (old_state == LifecycleState::kPrerendering &&
      new_state == LifecycleState::kActive) {
    for (auto& kv : iter->second->js_to_browser_messagings()) {
      kv.second->OnRenderFrameHostActivated();
    }
  }
}

void JsCommunicationHost::NotifyFrameForAllPersistentJavaScripts(
    content::RenderFrameHost* render_frame_host) {
  for (const auto& script : sticky_scripts_) {
    NotifyFrameForPersistentJavaScript(&script, render_frame_host);
  }
}

void JsCommunicationHost::NotifyFrameForWebMessageListener(
    content::RenderFrameHost* render_frame_host) {
  // AddWebMessageHostFactory() uses this method with ForEachFrame() from JNI.
  // Old entries are deleted from `js_to_browser_messagings_` by
  // RenderFrameDeleted(); however, RenderFrameDeleted() will not be called if
  // there is no live RenderFrame.
  if (!render_frame_host->IsRenderFrameLive())
    return;

  mojo::AssociatedRemote<mojom::JsCommunication> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  std::vector<mojom::JsObjectPtr> js_objects;
  js_objects.reserve(js_objects_.size());
  std::map<std::pair<std::u16string, int32_t>,
           std::unique_ptr<JsToBrowserMessaging>>
      js_to_browser_messagings;
  for (const auto& js_object : js_objects_) {
    if (NavigationWebMessageSender::IsNavigationListener(js_object->name)) {
      // This is the special navigationListener object that is registered to
      // listen for navigation events instead of establishing a connection to
      // the renderer. Don't create an object in the renderer. The
      // NavigationWebMessageSender for  `render_frame_host`'s Page should
      // either already be created when the object is first registered (see
      // `AddWebMessageHostFactory()`) or when the Page becomes the primary Page
      // (see `PrimaryPageChanged()`).
      // TODO(https://crbug.com/332809183): Guard this behind an origin trial
      // check later on.
      CHECK(has_navigation_listener_);
      continue;
    }
    mojo::PendingAssociatedRemote<mojom::JsToBrowserMessaging> pending_remote;
    mojo::PendingAssociatedReceiver<mojom::BrowserToJsMessagingFactory> factory;
    js_to_browser_messagings[{js_object->name, js_object->world_id}] =
        std::make_unique<JsToBrowserMessaging>(
            render_frame_host,
            pending_remote.InitWithNewEndpointAndPassReceiver(),
            factory.InitWithNewEndpointAndPassRemote(),
            js_object->factory.get(), js_object->allowed_origin_rules);
    js_objects.push_back(mojom::JsObject::New(
        js_object->name, std::move(pending_remote), std::move(factory),
        js_object->allowed_origin_rules, js_object->world_id));
  }
  mojo::PendingAssociatedRemote<mojom::JsObjectsClient> client;
  js_to_browser_messagings_[render_frame_host->GetGlobalId()] =
      std::make_unique<JsToBrowserMessagingList>(
          std::move(js_to_browser_messagings),
          client.InitWithNewEndpointAndPassReceiver());
  configurator_remote->SetJsObjects(std::move(js_objects), std::move(client));
}

void JsCommunicationHost::PrimaryPageChanged(content::Page& page) {
  // TODO(https://crbug.com/332809183): Guard this behind an origin trial check
  // later on.
  if (!base::FeatureList::IsEnabled(features::kEnableNavigationListener) ||
      !has_navigation_listener_) {
    return;
  }
  for (const auto& js_object : js_objects_) {
    // The active Page in the primary main frame just changed. Ensure that a
    // NavigationWebMessageSender is created for the primary Page, so that
    // navigation notifications for it will be sent correctly, including the
    // navigation that committed the primary Page. Note that some Pages
    // might not be primary even when navigations happen on them (e.g.
    // prerendering Pages), but we won't send notifications for those pages,
    // so there is no need to create the NavigationWebMessageSenders for
    // them before they become the primary Page.
    NavigationWebMessageSender::CreateForPageIfNeeded(page, js_object->name,
                                                      js_object->factory.get());
  }
}

void JsCommunicationHost::NotifyFrameForPersistentJavaScript(
    const JavaScriptExecutable* script,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(script);
  mojo::AssociatedRemote<mojom::JsCommunication> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  configurator_remote->AddPersistentJavaScript(mojom::JavaScriptExecutable::New(
      script->script_id_, script->script_, script->allowed_origin_rules_,
      script->event_type_, script->world_identifier_));
}

void JsCommunicationHost::NotifyFrameForRemovePersistentJavaScript(
    int32_t script_id,
    content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<mojom::JsCommunication> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  configurator_remote->RemovePersistentJavaScript(script_id);
}

}  // namespace js_injection
