// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/protocol_handler_registry.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/custom_handlers/pref_names.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

using content::BrowserThread;

namespace custom_handlers {

base::DictValue GetProtocolHandlerValue(
    const std::string& protocol,
    const std::string& url,
    bool is_confirmed = true,
    std::optional<std::string> app_id = std::nullopt,
    std::optional<std::string> extension_id = std::nullopt) {
  base::DictValue value;
  value.Set("protocol", protocol);
  value.Set("url", url);
  value.Set("is_confirmed", is_confirmed);
  if (app_id.has_value()) {
    value.Set("app_id", *app_id);
  }
  if (extension_id.has_value()) {
    value.Set("extension_id", *extension_id);
  }
  return value;
}

base::DictValue GetProtocolHandlerValueWithDefault(
    const std::string& protocol,
    const std::string& url,
    bool is_default,
    bool is_confirmed = true,
    std::optional<std::string> app_id = std::nullopt,
    std::optional<std::string> extension_id = std::nullopt) {
  base::DictValue value = GetProtocolHandlerValue(protocol, url, is_confirmed,
                                                  app_id, extension_id);
  value.Set("default", is_default);
  return value;
}

class ProtocolHandlerChangeListener : public ProtocolHandlerRegistry::Observer {
 public:
  explicit ProtocolHandlerChangeListener(ProtocolHandlerRegistry* registry) {
    registry_observation_.Observe(registry);
  }

  ProtocolHandlerChangeListener(const ProtocolHandlerChangeListener&) = delete;
  ProtocolHandlerChangeListener& operator=(
      const ProtocolHandlerChangeListener&) = delete;

  ~ProtocolHandlerChangeListener() override = default;

  int events() { return events_; }
  bool notified() { return events_ > 0; }
  void Clear() { events_ = 0; }

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override { ++events_; }

 private:
  int events_ = 0;

  base::ScopedObservation<ProtocolHandlerRegistry,
                          ProtocolHandlerRegistry::Observer>
      registry_observation_{this};
};

class QueryProtocolHandlerOnChange : public ProtocolHandlerRegistry::Observer {
 public:
  explicit QueryProtocolHandlerOnChange(ProtocolHandlerRegistry* registry)
      : local_registry_(registry) {
    registry_observation_.Observe(registry);
  }

  QueryProtocolHandlerOnChange(const QueryProtocolHandlerOnChange&) = delete;
  QueryProtocolHandlerOnChange& operator=(const QueryProtocolHandlerOnChange&) =
      delete;

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override {
    std::vector<std::string> output;
    local_registry_->GetRegisteredProtocols(&output);
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  raw_ptr<ProtocolHandlerRegistry> local_registry_;
  bool called_ = false;

  base::ScopedObservation<ProtocolHandlerRegistry,
                          ProtocolHandlerRegistry::Observer>
      registry_observation_{this};
};

class ProtocolHandlerRegistryTest : public testing::Test {
 protected:
  ProtocolHandlerRegistryTest()
      : test_protocol_handler_(CreateProtocolHandler("news", "news")) {}

  TestProtocolHandlerRegistryDelegate* delegate() const { return delegate_; }
  ProtocolHandlerRegistry* registry() { return registry_.get(); }
  const ProtocolHandler& test_protocol_handler() const {
    return test_protocol_handler_;
  }

  PrefService* GetPrefs() {
    DCHECK(browser_context_);
    return user_prefs::UserPrefs::Get(browser_context_.get());
  }

  ProtocolHandler CreateProtocolHandler(
      const std::string& protocol,
      const GURL& url,
      blink::ProtocolHandlerSecurityLevel security_level =
          blink::ProtocolHandlerSecurityLevel::kStrict) {
    return ProtocolHandler::CreateProtocolHandler(protocol, url,
                                                  security_level);
  }

  ProtocolHandler CreateProtocolHandler(const std::string& protocol,
                                        const std::string& name) {
    return CreateProtocolHandler(protocol, GURL("https://" + name + "/%s"));
  }

  ProtocolHandler CreateWebAppProtocolHandler(const std::string& protocol,
                                              const GURL& url,
                                              const std::string& app_id) {
    return ProtocolHandler::CreateWebAppProtocolHandler(protocol, url, app_id);
  }

  ProtocolHandler CreateExtensionProtocolHandler(
      const std::string& protocol,
      const GURL& url,
      const std::string& extension_id,
      bool is_allowed_in_incognito = false) {
    return ProtocolHandler::CreateExtensionProtocolHandler(
        protocol, url, extension_id, is_allowed_in_incognito);
  }

  bool ProtocolHandlerCanRegisterProtocol(
      const std::string& protocol,
      const GURL& handler_url,
      blink::ProtocolHandlerSecurityLevel security_level) {
    registry()->OnAcceptRegisterProtocolHandler(
        CreateProtocolHandler(protocol, handler_url, security_level));
    return registry()->IsHandledProtocol(protocol);
  }

  void RecreateRegistry(bool initialize, bool is_off_the_record = false) {
    TeadDownRegistry();
    SetUpRegistry(initialize, is_off_the_record);
  }

  // Replaces the registry with a freshly constructed OTR one that loads from
  // the same PrefService. Handlers persisted by the previous (regular)
  // registry are re-loaded under the insertion guard, so disallowed entries
  // are filtered out at load time.
  void SwitchToIncognito() {
    RecreateRegistry(/*initialize=*/true, /*is_off_the_record=*/true);
  }

  int InPrefHandlerCount() {
    const base::ListValue& in_pref_handlers = GetPrefs()->GetList(
        custom_handlers::prefs::kRegisteredProtocolHandlers);
    return static_cast<int>(in_pref_handlers.size());
  }

  int InMemoryHandlerCount() {
    int in_memory_handler_count = 0;
    auto it = registry()->protocol_handlers_.begin();
    for (; it != registry()->protocol_handlers_.end(); ++it)
      in_memory_handler_count += it->second.size();
    return in_memory_handler_count;
  }

  int InPrefIgnoredHandlerCount() {
    const base::ListValue& in_pref_ignored_handlers =
        GetPrefs()->GetList(custom_handlers::prefs::kIgnoredProtocolHandlers);
    return static_cast<int>(in_pref_ignored_handlers.size());
  }

  int InMemoryIgnoredHandlerCount() {
    int in_memory_ignored_handler_count = 0;
    auto it = registry()->ignored_protocol_handlers_.begin();
    for (; it != registry()->ignored_protocol_handlers_.end(); ++it)
      in_memory_ignored_handler_count++;
    return in_memory_ignored_handler_count;
  }

  // It creates a new instance of the ProtocolHandlerRegistry class,
  // initializing it if |initialize| is true, for the registry_ member variable.
  void SetUpRegistry(bool initialize, bool is_off_the_record = false) {
    DCHECK(browser_context_);
    auto delegate = std::make_unique<TestProtocolHandlerRegistryDelegate>();
    delegate_ = delegate.get();
    registry_ = std::make_unique<ProtocolHandlerRegistry>(
        GetPrefs(), std::move(delegate), is_off_the_record);
    if (initialize)
      registry_->InitProtocolSettings();
  }

  void TeadDownRegistry() {
    delegate_ = nullptr;
    registry_->Shutdown();
    registry_.reset();
  }

  void SetUp() override {
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    user_prefs::UserPrefs::Set(browser_context_.get(), &pref_service_);
    ProtocolHandlerRegistry::RegisterProfilePrefs(pref_service_.registry());
    CHECK(GetPrefs());
    SetUpRegistry(true);
    test_protocol_handler_ =
        CreateProtocolHandler("news", GURL("https://test.com/%s"));
  }

  void TearDown() override { TeadDownRegistry(); }

  sync_preferences::TestingPrefServiceSyncable& testing_pref_service() {
    return pref_service_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<content::TestBrowserContext> browser_context_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  raw_ptr<TestProtocolHandlerRegistryDelegate>
      delegate_;  // Registry assumes ownership of delegate_.
  std::unique_ptr<ProtocolHandlerRegistry> registry_;
  ProtocolHandler test_protocol_handler_;
};

TEST_F(ProtocolHandlerRegistryTest, AcceptProtocolHandlerHandlesProtocol) {
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, DeniedProtocolIsntHandledUntilAccepted) {
  registry()->OnDenyRegisterProtocolHandler(test_protocol_handler());
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, ClearDefaultMakesProtocolNotHandled) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  registry()->ClearDefault("news");
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  ASSERT_TRUE(registry()->GetHandlerFor("news").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, DisableDeregistersProtocolHandlers) {
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("news"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("news"));

  registry()->Disable();
  ASSERT_FALSE(delegate()->IsExternalHandlerRegistered("news"));
  registry()->Enable();
  ASSERT_TRUE(delegate()->IsExternalHandlerRegistered("news"));
}

TEST_F(ProtocolHandlerRegistryTest, IgnoreProtocolHandler) {
  registry()->OnIgnoreRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsIgnored(test_protocol_handler()));

  registry()->RemoveIgnoredHandler(test_protocol_handler());
  ASSERT_FALSE(registry()->IsIgnored(test_protocol_handler()));
}

TEST_F(ProtocolHandlerRegistryTest, IgnoreEquivalentProtocolHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", GURL("https://test/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("news", GURL("https://test/%s"));

  registry()->OnIgnoreRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsIgnored(ph1));
  ASSERT_TRUE(registry()->HasIgnoredEquivalent(ph2));

  registry()->RemoveIgnoredHandler(ph1);
  ASSERT_FALSE(registry()->IsIgnored(ph1));
  ASSERT_FALSE(registry()->HasIgnoredEquivalent(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, SaveAndLoad) {
  ProtocolHandler stuff_protocol_handler(
      CreateProtocolHandler("stuff", "stuff"));
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  registry()->OnIgnoreRegisterProtocolHandler(stuff_protocol_handler);

  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  ASSERT_TRUE(registry()->IsIgnored(stuff_protocol_handler));
  delegate()->Reset();
  RecreateRegistry(true);
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  ASSERT_TRUE(registry()->IsIgnored(stuff_protocol_handler));
}

TEST_F(ProtocolHandlerRegistryTest, Encode) {
  base::Time now = base::Time::Now();
  ProtocolHandler handler("news", GURL("https://example.com"), "app_id",
                          std::nullopt, now, true, true,
                          blink::ProtocolHandlerSecurityLevel::kStrict);
  auto value = handler.Encode();
  ProtocolHandler recreated = ProtocolHandler::CreateProtocolHandler(value);
  EXPECT_EQ("news", recreated.protocol());
  EXPECT_EQ(GURL("https://example.com"), recreated.url());
  EXPECT_EQ(now, recreated.last_modified());
}

// CreateProtocolHandler must default is_allowed_in_incognito to false.
// Incognito access is opt-in; a freshly registered handler must not be
// silently visible in incognito before the user grants that permission.
TEST_F(ProtocolHandlerRegistryTest,
       CreateProtocolHandlerDefaultsToNotAllowedInIncognito) {
  ProtocolHandler handler =
      CreateProtocolHandler("news", GURL("https://test.com/%s"));
  EXPECT_FALSE(handler.is_allowed_in_incognito());
}

// Encode() followed by CreateProtocolHandler(DictValue) must round-trip
// is_allowed_in_incognito for all three handler types (plain web, web app,
// extension).  This guards against deserialization paths that silently drop
// the flag and fall back to the field initializer.
TEST_F(ProtocolHandlerRegistryTest, EncodeRoundtripsIsAllowedInIncognito) {
  base::Time now = base::Time::Now();
  const blink::ProtocolHandlerSecurityLevel kStrict =
      blink::ProtocolHandlerSecurityLevel::kStrict;

  struct {
    std::optional<std::string> app_id;
    std::optional<std::string> extension_id;
  } cases[] = {
      {std::nullopt, std::nullopt},  // plain web handler
      {"app_id_123", std::nullopt},  // web app handler
      {std::nullopt, "ext_id_456"},  // extension handler
  };

  for (const auto& c : cases) {
    for (bool allowed : {false, true}) {
      ProtocolHandler handler("news", GURL("https://example.com/%s"), c.app_id,
                              c.extension_id, now,
                              /*is_confirmed=*/true, allowed, kStrict);
      ProtocolHandler recreated =
          ProtocolHandler::CreateProtocolHandler(handler.Encode());
      EXPECT_EQ(allowed, recreated.is_allowed_in_incognito());
    }
  }
}

TEST_F(ProtocolHandlerRegistryTest, GetHandlersBetween) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago = now - base::Hours(1);
  base::Time two_hours_ago = now - base::Hours(2);
  ProtocolHandler handler1("bitcoin", GURL("https://example.com"),
                           two_hours_ago,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  ProtocolHandler handler2("geo", GURL("https://example.com"), one_hour_ago,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  ProtocolHandler handler3("im", GURL("https://example.com"), now,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  registry()->OnAcceptRegisterProtocolHandler(handler1);
  registry()->OnAcceptRegisterProtocolHandler(handler2);
  registry()->OnAcceptRegisterProtocolHandler(handler3);

  EXPECT_EQ(
      std::vector<ProtocolHandler>({handler1, handler2, handler3}),
      registry()->GetUserDefinedHandlers(base::Time(), base::Time::Max()));
  EXPECT_EQ(
      std::vector<ProtocolHandler>({handler2, handler3}),
      registry()->GetUserDefinedHandlers(one_hour_ago, base::Time::Max()));
  EXPECT_EQ(std::vector<ProtocolHandler>({handler1, handler2}),
            registry()->GetUserDefinedHandlers(base::Time(), now));
}

TEST_F(ProtocolHandlerRegistryTest, ClearHandlersBetween) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago = now - base::Hours(1);
  base::Time two_hours_ago = now - base::Hours(2);
  GURL url("https://example.com");
  ProtocolHandler handler1("bitcoin", url, two_hours_ago,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  ProtocolHandler handler2("geo", url, one_hour_ago,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  ProtocolHandler handler3("im", url, now,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  ProtocolHandler ignored1("irc", url, two_hours_ago,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  ProtocolHandler ignored2("ircs", url, one_hour_ago,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  ProtocolHandler ignored3("magnet", url, now,
                           blink::ProtocolHandlerSecurityLevel::kStrict);
  registry()->OnAcceptRegisterProtocolHandler(handler1);
  registry()->OnAcceptRegisterProtocolHandler(handler2);
  registry()->OnAcceptRegisterProtocolHandler(handler3);
  registry()->OnIgnoreRegisterProtocolHandler(ignored1);
  registry()->OnIgnoreRegisterProtocolHandler(ignored2);
  registry()->OnIgnoreRegisterProtocolHandler(ignored3);

  EXPECT_TRUE(registry()->IsHandledProtocol("bitcoin"));
  EXPECT_TRUE(registry()->IsHandledProtocol("geo"));
  EXPECT_TRUE(registry()->IsHandledProtocol("im"));
  EXPECT_TRUE(registry()->IsIgnored(ignored1));
  EXPECT_TRUE(registry()->IsIgnored(ignored2));
  EXPECT_TRUE(registry()->IsIgnored(ignored3));

  // Delete handler2 and ignored2.
  registry()->ClearUserDefinedHandlers(one_hour_ago, now);
  EXPECT_TRUE(registry()->IsHandledProtocol("bitcoin"));
  EXPECT_FALSE(registry()->IsHandledProtocol("geo"));
  EXPECT_TRUE(registry()->IsHandledProtocol("im"));
  EXPECT_TRUE(registry()->IsIgnored(ignored1));
  EXPECT_FALSE(registry()->IsIgnored(ignored2));
  EXPECT_TRUE(registry()->IsIgnored(ignored3));

  // Delete all.
  registry()->ClearUserDefinedHandlers(base::Time(), base::Time::Max());
  EXPECT_FALSE(registry()->IsHandledProtocol("bitcoin"));
  EXPECT_FALSE(registry()->IsHandledProtocol("geo"));
  EXPECT_FALSE(registry()->IsHandledProtocol("im"));
  EXPECT_FALSE(registry()->IsIgnored(ignored1));
  EXPECT_FALSE(registry()->IsIgnored(ignored2));
  EXPECT_FALSE(registry()->IsIgnored(ignored3));
}

TEST_F(ProtocolHandlerRegistryTest, TestExtensionProtocolHandlers) {
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  ProtocolHandler ph1 =
      CreateExtensionProtocolHandler("news", GURL("https://test/%s"), kIdFoo);
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->HasDefaultHandler("news"));
  ASSERT_TRUE(registry()->IsDefault(ph1));

  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  ProtocolHandler ph2 =
      CreateExtensionProtocolHandler("mailto", GURL("https://test/%s"), kIdBar);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_TRUE(registry()->HasDefaultHandler("mailto"));
  ASSERT_TRUE(registry()->IsDefault(ph2));

  {
    ProtocolHandlerRegistry::ProtocolHandlerList handlers =
        registry()->GetExtensionProtocolHandlers();
    ASSERT_EQ(static_cast<size_t>(2), handlers.size());
  }

  {
    ProtocolHandlerRegistry::ProtocolHandlerList handlers =
        registry()->GetExtensionProtocolHandlers(kIdBar);
    ASSERT_EQ(static_cast<size_t>(1), handlers.size());
  }
}

TEST_F(ProtocolHandlerRegistryTest, TestEnabledDisabled) {
  registry()->Disable();
  ASSERT_FALSE(registry()->enabled());
  registry()->Enable();
  ASSERT_TRUE(registry()->enabled());
}

TEST_F(ProtocolHandlerRegistryTest,
       DisallowRegisteringExternallyHandledProtocols) {
  ASSERT_TRUE(!delegate()->IsExternalHandlerRegistered("news"));
  delegate()->RegisterExternalHandler("news");
  ASSERT_FALSE(registry()->CanSchemeBeOverridden("news"));
}

TEST_F(ProtocolHandlerRegistryTest, RemovingHandlerMeansItCanBeAddedAgain) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->CanSchemeBeOverridden("news"));
  registry()->RemoveHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->CanSchemeBeOverridden("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestStartsAsDefault) {
  registry()->OnAcceptRegisterProtocolHandler(test_protocol_handler());
  ASSERT_TRUE(registry()->IsDefault(test_protocol_handler()));
}

TEST_F(ProtocolHandlerRegistryTest, TestClearDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault("news");
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_FALSE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestGetHandlerFor) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_EQ(ph2, registry()->GetHandlerFor("news"));
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestMostRecentHandlerIsDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestOnAcceptRegisterProtocolHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsDefault(ph1));
  ASSERT_FALSE(registry()->IsDefault(ph2));

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestDefaultSaveLoad) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnDenyRegisterProtocolHandler(ph1);
  registry()->OnDenyRegisterProtocolHandler(ph2);

  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->Disable();

  RecreateRegistry(true);

  ASSERT_FALSE(registry()->enabled());
  registry()->Enable();
  ASSERT_FALSE(registry()->IsDefault(ph1));
  ASSERT_TRUE(registry()->IsDefault(ph2));

  RecreateRegistry(true);
  ASSERT_TRUE(registry()->enabled());
}

TEST_F(ProtocolHandlerRegistryTest, TestRemoveHandler) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));

  registry()->OnIgnoreRegisterProtocolHandler(ph1);
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->IsIgnored(ph1));

  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_FALSE(registry()->IsIgnored(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestIsRegistered) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  ASSERT_TRUE(registry()->IsRegistered(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestIsEquivalentRegistered) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", GURL("https://test/%s"));
  ProtocolHandler ph2 = CreateProtocolHandler("news", GURL("https://test/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);

  ASSERT_TRUE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->HasRegisteredEquivalent(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestSilentlyRegisterHandler) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("news", GURL("https://test/1/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("news", GURL("https://test/2/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("ignore", GURL("https://test/%s"));
  ProtocolHandler ph4 =
      CreateProtocolHandler("ignore", GURL("https://test/%s"));

  ASSERT_FALSE(registry()->SilentlyHandleRegisterHandlerRequest(ph1));
  ASSERT_FALSE(registry()->IsRegistered(ph1));

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsRegistered(ph1));

  ASSERT_TRUE(registry()->SilentlyHandleRegisterHandlerRequest(ph2));
  ASSERT_FALSE(registry()->IsRegistered(ph1));
  ASSERT_TRUE(registry()->IsRegistered(ph2));

  ASSERT_FALSE(registry()->SilentlyHandleRegisterHandlerRequest(ph3));
  ASSERT_FALSE(registry()->IsRegistered(ph3));

  registry()->OnIgnoreRegisterProtocolHandler(ph3);
  ASSERT_FALSE(registry()->IsRegistered(ph3));
  ASSERT_TRUE(registry()->IsIgnored(ph3));

  ASSERT_TRUE(registry()->SilentlyHandleRegisterHandlerRequest(ph4));
  ASSERT_FALSE(registry()->IsRegistered(ph4));
  ASSERT_TRUE(registry()->HasIgnoredEquivalent(ph4));
}

TEST_F(ProtocolHandlerRegistryTest, TestRemoveHandlerRemovesDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("news", "test3");

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->RemoveHandler(ph1);
  ASSERT_FALSE(registry()->IsDefault(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestGetHandlersFor) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("news", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("news", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      registry()->GetHandlersFor("news");
  ASSERT_EQ(static_cast<size_t>(3), handlers.size());

  ASSERT_EQ(ph3, handlers[0]);
  ASSERT_EQ(ph2, handlers[1]);
  ASSERT_EQ(ph1, handlers[2]);
}

TEST_F(ProtocolHandlerRegistryTest, TestGetRegisteredProtocols) {
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(0), protocols.size());

  registry()->GetHandlersFor("news");

  protocols.clear();
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(0), protocols.size());
}

TEST_F(ProtocolHandlerRegistryTest, TestIsHandledProtocol) {
  registry()->GetHandlersFor("news");
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestObserver) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  ProtocolHandlerChangeListener counter(registry());

  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->Disable();
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->Enable();
  ASSERT_TRUE(counter.notified());
  counter.Clear();

  registry()->RemoveHandler(ph1);
  ASSERT_TRUE(counter.notified());
  counter.Clear();
}

TEST_F(ProtocolHandlerRegistryTest, TestReentrantObserver) {
  QueryProtocolHandlerOnChange queryer(registry());
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(queryer.called());
}

TEST_F(ProtocolHandlerRegistryTest, TestProtocolsWithNoDefaultAreHandled) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->ClearDefault("news");
  std::vector<std::string> handled_protocols;
  registry()->GetRegisteredProtocols(&handled_protocols);
  ASSERT_EQ(static_cast<size_t>(1), handled_protocols.size());
  ASSERT_EQ("news", handled_protocols[0]);
}

TEST_F(ProtocolHandlerRegistryTest, TestDisablePreventsHandling) {
  ProtocolHandler ph1 = CreateProtocolHandler("news", "test1");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  registry()->Disable();
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, TestOSRegistration) {
  ProtocolHandler ph_do1 = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph_do2 = CreateProtocolHandler("news", "test2");
  ProtocolHandler ph_dont = CreateProtocolHandler("im", "test3");

  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("news"));
  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("im"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do1);
  registry()->OnDenyRegisterProtocolHandler(ph_dont);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(delegate()->IsFakeRegisteredWithOS("news"));
  ASSERT_FALSE(delegate()->IsFakeRegisteredWithOS("im"));

  // This should not register with the OS, if it does the delegate
  // will assert for us. We don't need to wait for the message loop
  // as it should not go through to the shell worker.
  registry()->OnAcceptRegisterProtocolHandler(ph_do2);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// TODO(benwells): When Linux support is more reliable and
// http://crbug.com/88255 is fixed this test will pass.
#define MAYBE_TestOSRegistrationFailure DISABLED_TestOSRegistrationFailure
#else
#define MAYBE_TestOSRegistrationFailure TestOSRegistrationFailure
#endif

TEST_F(ProtocolHandlerRegistryTest, MAYBE_TestOSRegistrationFailure) {
  ProtocolHandler ph_do = CreateProtocolHandler("news", "test1");
  ProtocolHandler ph_dont = CreateProtocolHandler("im", "test2");

  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  ASSERT_FALSE(registry()->IsHandledProtocol("im"));

  registry()->OnAcceptRegisterProtocolHandler(ph_do);
  base::RunLoop().RunUntilIdle();

  delegate()->set_force_os_failure(true);
  registry()->OnAcceptRegisterProtocolHandler(ph_dont);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(registry()->IsHandledProtocol("news"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("news").size());
  ASSERT_FALSE(registry()->IsHandledProtocol("im"));
  ASSERT_EQ(static_cast<size_t>(1), registry()->GetHandlersFor("im").size());
}

TEST_F(ProtocolHandlerRegistryTest, TestRemovingDefaultFallsBackToOldDefault) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);

  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph2));
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph2);
  ASSERT_TRUE(registry()->IsDefault(ph3));
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->IsDefault(ph1));
}

TEST_F(ProtocolHandlerRegistryTest, TestRemovingDefaultDoesntChangeHandlers) {
  ProtocolHandler ph1 = CreateProtocolHandler("mailto", "test1");
  ProtocolHandler ph2 = CreateProtocolHandler("mailto", "test2");
  ProtocolHandler ph3 = CreateProtocolHandler("mailto", "test3");
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  registry()->RemoveHandler(ph3);

  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      registry()->GetHandlersFor("mailto");
  ASSERT_EQ(static_cast<size_t>(2), handlers.size());

  ASSERT_EQ(ph2, handlers[0]);
  ASSERT_EQ(ph1, handlers[1]);
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceHandler) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("https://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("https://test.com/updated-url/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph2.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceNonDefaultHandler) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("https://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("https://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("https://else.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph3);
  ASSERT_TRUE(registry()->AttemptReplace(ph2));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph3.url());
}

TEST_F(ProtocolHandlerRegistryTest, TestReplaceRemovesStaleHandlers) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("https://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("https://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("https://test.com/third/%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph1);
  registry()->OnAcceptRegisterProtocolHandler(ph2);

  // This should replace the previous two handlers.
  ASSERT_TRUE(registry()->AttemptReplace(ph3));
  const ProtocolHandler& handler(registry()->GetHandlerFor("mailto"));
  ASSERT_EQ(handler.url(), ph3.url());
  registry()->RemoveHandler(ph3);
  ASSERT_TRUE(registry()->GetHandlerFor("mailto").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, TestIsSameOrigin) {
  ProtocolHandler ph1 =
      CreateProtocolHandler("mailto", GURL("https://test.com/%s"));
  ProtocolHandler ph2 =
      CreateProtocolHandler("mailto", GURL("https://test.com/updated-url/%s"));
  ProtocolHandler ph3 =
      CreateProtocolHandler("mailto", GURL("https://other.com/%s"));
  ASSERT_EQ(ph1.url().DeprecatedGetOriginAsURL() ==
                ph2.url().DeprecatedGetOriginAsURL(),
            ph1.IsSameOrigin(ph2));
  ASSERT_EQ(ph1.url().DeprecatedGetOriginAsURL() ==
                ph2.url().DeprecatedGetOriginAsURL(),
            ph2.IsSameOrigin(ph1));
  ASSERT_EQ(ph2.url().DeprecatedGetOriginAsURL() ==
                ph3.url().DeprecatedGetOriginAsURL(),
            ph2.IsSameOrigin(ph3));
  ASSERT_EQ(ph3.url().DeprecatedGetOriginAsURL() ==
                ph2.url().DeprecatedGetOriginAsURL(),
            ph3.IsSameOrigin(ph2));
}

TEST_F(ProtocolHandlerRegistryTest, TestInstallDefaultHandler) {
  RecreateRegistry(false);
  registry()->AddPredefinedHandler(
      CreateProtocolHandler("news", GURL("https://test.com/%s")));
  registry()->InitProtocolSettings();
  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  ASSERT_EQ(static_cast<size_t>(1), protocols.size());
  EXPECT_TRUE(registry()->IsHandledProtocol("news"));
  auto handlers =
      registry()->GetUserDefinedHandlers(base::Time(), base::Time::Max());
  EXPECT_TRUE(handlers.empty());
  registry()->ClearUserDefinedHandlers(base::Time(), base::Time::Max());
  EXPECT_TRUE(registry()->IsHandledProtocol("news"));
}

#define URL_p1u1 "https://p1u1.com/%s"
#define URL_p1u2 "https://p1u2.com/%s"
#define URL_p1u3 "https://p1u3.com/%s"
#define URL_p2u1 "https://p2u1.com/%s"
#define URL_p2u2 "https://p2u2.com/%s"
#define URL_p3u1 "https://p3u1.com/%s"

TEST_F(ProtocolHandlerRegistryTest, TestPrefPolicyOverlapRegister) {
  base::ListValue handlers_registered_by_pref;
  base::ListValue handlers_registered_by_policy;

  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u2, true));
  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u1, true));
  handlers_registered_by_pref.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u2, false));

  handlers_registered_by_policy.Append(
      GetProtocolHandlerValueWithDefault("news", URL_p1u1, false));
  handlers_registered_by_policy.Append(
      GetProtocolHandlerValueWithDefault("mailto", URL_p3u1, true));

  GetPrefs()->SetList(custom_handlers::prefs::kRegisteredProtocolHandlers,
                      std::move(handlers_registered_by_pref));
  GetPrefs()->SetList(custom_handlers::prefs::kPolicyRegisteredProtocolHandlers,
                      std::move(handlers_registered_by_policy));
  registry()->InitProtocolSettings();

  // Duplicate p1u2 eliminated in memory but not yet saved in pref
  ProtocolHandler p1u1 = CreateProtocolHandler("news", GURL(URL_p1u1));
  ProtocolHandler p1u2 = CreateProtocolHandler("news", GURL(URL_p1u2));
  ASSERT_EQ(InPrefHandlerCount(), 3);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_TRUE(registry()->IsDefault(p1u1));
  ASSERT_FALSE(registry()->IsDefault(p1u2));

  ProtocolHandler p2u1 = CreateProtocolHandler("im", GURL(URL_p2u1));
  registry()->OnDenyRegisterProtocolHandler(p2u1);

  // Duplicate p1u2 saved in pref and a new handler added to pref and memory
  ASSERT_EQ(InPrefHandlerCount(), 3);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_FALSE(registry()->IsDefault(p2u1));

  registry()->RemoveHandler(p1u1);

  // p1u1 removed from user pref but not from memory due to policy.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_TRUE(registry()->IsDefault(p1u1));

  ProtocolHandler p3u1 = CreateProtocolHandler("mailto", GURL(URL_p3u1));
  registry()->RemoveHandler(p3u1);

  // p3u1 not removed from memory due to policy and it was never in pref.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_TRUE(registry()->IsDefault(p3u1));

  registry()->RemoveHandler(p1u2);

  // p1u2 removed from user pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 1);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_TRUE(registry()->IsDefault(p1u1));

  ProtocolHandler p1u3 = CreateProtocolHandler("news", GURL(URL_p1u3));
  registry()->OnAcceptRegisterProtocolHandler(p1u3);

  // p1u3 added to pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 2);
  ASSERT_EQ(InMemoryHandlerCount(), 4);
  ASSERT_FALSE(registry()->IsDefault(p1u1));
  ASSERT_TRUE(registry()->IsDefault(p1u3));

  registry()->RemoveHandler(p1u3);

  // p1u3 the default handler for p1 removed from user pref and memory.
  ASSERT_EQ(InPrefHandlerCount(), 1);
  ASSERT_EQ(InMemoryHandlerCount(), 3);
  ASSERT_FALSE(registry()->IsDefault(p1u3));
  ASSERT_TRUE(registry()->IsDefault(p1u1));
  ASSERT_TRUE(registry()->IsDefault(p3u1));
  ASSERT_FALSE(registry()->IsDefault(p2u1));
}

TEST_F(ProtocolHandlerRegistryTest, TestPrefPolicyOverlapIgnore) {
  base::ListValue handlers_ignored_by_pref;
  base::ListValue handlers_ignored_by_policy;

  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("news", URL_p1u1));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("news", URL_p1u2));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("news", URL_p1u2));
  handlers_ignored_by_pref.Append(GetProtocolHandlerValue("mailto", URL_p3u1));

  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("news", URL_p1u2));
  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("news", URL_p1u3));
  handlers_ignored_by_policy.Append(GetProtocolHandlerValue("im", URL_p2u1));

  GetPrefs()->SetList(custom_handlers::prefs::kIgnoredProtocolHandlers,
                      std::move(handlers_ignored_by_pref));
  GetPrefs()->SetList(custom_handlers::prefs::kPolicyIgnoredProtocolHandlers,
                      std::move(handlers_ignored_by_policy));
  registry()->InitProtocolSettings();

  // Duplicate p1u2 eliminated in memory but not yet saved in pref
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 5);

  ProtocolHandler p2u2 = CreateProtocolHandler("im", GURL(URL_p2u2));
  registry()->OnIgnoreRegisterProtocolHandler(p2u2);

  // Duplicate p1u2 eliminated in pref, p2u2 added to pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p2u1 = CreateProtocolHandler("im", GURL(URL_p2u1));
  registry()->RemoveIgnoredHandler(p2u1);

  // p2u1 installed by policy so cant be removed.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 4);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p1u2 = CreateProtocolHandler("news", GURL(URL_p1u2));
  registry()->RemoveIgnoredHandler(p1u2);

  // p1u2 installed by policy and pref so it is removed from pref and not from
  // memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 3);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 6);

  ProtocolHandler p1u1 = CreateProtocolHandler("news", GURL(URL_p1u1));
  registry()->RemoveIgnoredHandler(p1u1);

  // p1u1 installed by pref so it is removed from pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 2);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 5);

  registry()->RemoveIgnoredHandler(p2u2);

  // p2u2 installed by user so it is removed from pref and memory.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 1);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);

  registry()->OnIgnoreRegisterProtocolHandler(p2u1);

  // p2u1 installed by user but it is already installed by policy, so it is
  // added to pref.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 2);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);

  registry()->RemoveIgnoredHandler(p2u1);

  // p2u1 installed by user and policy, so it is removed from pref alone.
  ASSERT_EQ(InPrefIgnoredHandlerCount(), 1);
  ASSERT_EQ(InMemoryIgnoredHandlerCount(), 4);
}

TEST_F(ProtocolHandlerRegistryTest, TestURIPercentEncoding) {
  ProtocolHandler ph =
      CreateProtocolHandler("web+custom", GURL("https://test.com/url=%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph);

  // Normal case.
  GURL translated_url = ph.TranslateUrl(GURL("web+custom://custom/handler"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2Fhandler"));

  // Percent-encoding.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/%20handler"));
  ASSERT_EQ(
      translated_url,
      GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2F%2520handler"));

  // Percent-encoded spaces in the host part.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom%20handler"));
  ASSERT_EQ(
      translated_url,
      GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2520handler"));

  // Query parameters.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom?foo=bar&bar=baz"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%3Ffoo%3Dbar%26bar%3Dbaz"));

  // Non-ASCII characters.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/<>`{}#?\"'😂"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%2F%253C%253E%2560%257B%257D%"
                 "23%3F%2522'%25F0%259F%2598%2582"));

  // ASCII characters from the C0 controls percent-encode set.
  // GURL constructor encodes U+001F and U+007F as "%1F" and "%7F" first,
  // Then the protocol handler translator encodes them to "%25%1F" and "%25%7F"
  // again. That's why the expected result has double-encoded URL.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/\x1fhandler"));
  ASSERT_EQ(
      translated_url,
      GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2F%251Fhandler"));
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/\x7Fhandler"));
  ASSERT_EQ(
      translated_url,
      GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%2F%257Fhandler"));

  // Path percent-encode set.
  translated_url =
      ph.TranslateUrl(GURL("web+custom://custom/handler=#download"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%2Fhandler%3D%23download"));

  // Userinfo percent-encode set.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/handler:@id="));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%2Fhandler%3A%40id%3D"));
}

TEST_F(ProtocolHandlerRegistryTest, TestMultiplePlaceholders) {
  ProtocolHandler ph =
      CreateProtocolHandler("news", GURL("https://example.com/%s/url=%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph);

  GURL translated_url = ph.TranslateUrl(GURL("test:duplicated_placeholders"));

  // When URL contains multiple placeholders, only the first placeholder should
  // be changed to the given URL.
  ASSERT_EQ(translated_url,
            GURL("https://example.com/test%3Aduplicated_placeholders/url=%s"));
}

TEST_F(ProtocolHandlerRegistryTest, InvalidHandlers) {
  // Invalid protocol.
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("foo", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("foo"));
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("web", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web"));
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("web+", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web+"));
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("https", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("https"));

  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "ext+", GURL("https://www.google.com/handler%s"),
      blink::ProtocolHandlerSecurityLevel::kStrict));
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "ext+foo", GURL("https://www.google.com/handler%s"),
      blink::ProtocolHandlerSecurityLevel::kStrict));

  // Invalid handler URL.
  // data: URL.
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "news",
      GURL("data:text/html,<html><body><b>hello world</b></body></html>%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  // ftp:// URL.
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("news", GURL("ftp://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  // blob:// URL
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "news", GURL("blob:https://www.google.com/"
                   "f2d8c47d-17d0-4bf5-8f0a-76e42cbed3bf/%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  // http:// URL
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("news", GURL("http://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
  // filesystem:// URL
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "news", GURL("filesystem:https://www.google.com/"
                   "f2d8c47d-17d0-4bf5-8f0a-76e42cbed3bf/%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
}

// See
// https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
TEST_F(ProtocolHandlerRegistryTest, WebPlusPrefix) {
  // Not ASCII alphas.
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "web+***", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web+***"));
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "web+123", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web+123"));
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "web+   ", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web+   "));
  registry()->OnAcceptRegisterProtocolHandler(CreateProtocolHandler(
      "web+name123", GURL("https://www.google.com/handler%s")));
  ASSERT_FALSE(registry()->IsHandledProtocol("web+name123"));

  // ASCII lower alphas.
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("web+abcdefghijklmnopqrstuvwxyz",
                            GURL("https://www.google.com/handler%s")));
  ASSERT_TRUE(registry()->IsHandledProtocol("web+abcdefghijklmnopqrstuvwxyz"));

  // ASCII upper alphas are lowercased.
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("web+ZYXWVUTSRQPONMLKJIHGFEDCBA",
                            GURL("https://www.google.com/handler%s")));
  ASSERT_TRUE(registry()->IsHandledProtocol("web+zyxwvutsrqponmlkjihgfedcba"));
}

TEST_F(ProtocolHandlerRegistryTest, ProtocolHandlerSecurityLevels) {
  GURL https_handler_url("https://www.google.com/handler%s");

  // Invalid protocol.
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "foo", https_handler_url,
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "web", https_handler_url,
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "web+", https_handler_url,
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "https", https_handler_url,
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "ext+", https_handler_url,
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "ext+foo", https_handler_url,
      blink::ProtocolHandlerSecurityLevel::kUntrustedOrigins));

  // Invalid handler URL.
  // data: URL.
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news",
      GURL("data:text/html,<html><body><b>hello "
           "world</b></body></html>%s"),
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  // ftp:// URL.
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news", GURL("ftp://www.google.com/handler%s"),
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  // blob:// URL
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news",
      GURL("blob:https://www.google.com/"
           "f2d8c47d-17d0-4bf5-8f0a-76e42cbed3bf/%s"),
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  // http:// URL
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news", GURL("http://www.google.com/handler%s"),
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
  // filesystem:// URL
  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news",
      GURL("filesystem:https://www.google.com/"
           "f2d8c47d-17d0-4bf5-8f0a-76e42cbed3bf/%s"),
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));

  // ext+foo scheme.
  EXPECT_TRUE(ProtocolHandlerCanRegisterProtocol(
      "ext+foo", https_handler_url,
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
}

TEST_F(ProtocolHandlerRegistryTest, OnlyExtensionHandlersUnconfirmed) {
  registry()->OnAcceptRegisterProtocolHandler(
      CreateProtocolHandler("web+play", GURL("https://test/%s")));
  EXPECT_TRUE(registry()->IsProtocolHandlerConfirmed("web+play"));

  const std::string kIdFoo("fooId");
  registry()->OnAcceptRegisterProtocolHandler(
      CreateWebAppProtocolHandler("web+mail", GURL("https://test/%s"), kIdFoo));
  EXPECT_TRUE(registry()->IsProtocolHandlerConfirmed("web+mail"));

  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  registry()->OnAcceptRegisterProtocolHandler(CreateExtensionProtocolHandler(
      "web+news", GURL("https://test/%s"), kIdBar));
  EXPECT_FALSE(registry()->IsProtocolHandlerConfirmed("web+news"));
}

TEST_F(ProtocolHandlerRegistryTest, ConfirmHandler) {
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  registry()->OnAcceptRegisterProtocolHandler(CreateExtensionProtocolHandler(
      "web+news", GURL("https://test/%s"), kIdBar));
  EXPECT_FALSE(registry()->IsProtocolHandlerConfirmed("web+news"));

  registry()->ConfirmProtocolHandler("web+news", false /*save*/);
  EXPECT_TRUE(registry()->IsProtocolHandlerConfirmed("web+news"));
}

TEST_F(ProtocolHandlerRegistryTest, RestoreUnconfirmedHandlerFromPref) {
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  base::ListValue handlers_registered_by_pref;

  handlers_registered_by_pref.Append(GetProtocolHandlerValueWithDefault(
      "news", URL_p1u1, true, false, std::nullopt, kIdBar));

  GetPrefs()->SetList(custom_handlers::prefs::kRegisteredProtocolHandlers,
                      std::move(handlers_registered_by_pref));
  registry()->InitProtocolSettings();

  ASSERT_TRUE(registry()->HasDefaultHandler("news"));
  EXPECT_FALSE(registry()->IsProtocolHandlerConfirmed("news"));
}

TEST_F(ProtocolHandlerRegistryTest, ConfirmHandlerAndSave) {
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  registry()->OnAcceptRegisterProtocolHandler(CreateExtensionProtocolHandler(
      "web+news", GURL("https://test/%s"), kIdBar));
  EXPECT_FALSE(registry()->IsProtocolHandlerConfirmed("web+news"));

  registry()->ConfirmProtocolHandler("web+news", true /*save*/);
  EXPECT_TRUE(registry()->IsProtocolHandlerConfirmed("web+news"));

  // Restore the registry from prefs.
  delegate()->Reset();
  RecreateRegistry(true);
  EXPECT_TRUE(registry()->IsProtocolHandlerConfirmed("web+news"));
}

namespace {

enum class ProtocolTestMode {
  kPaytoOff,
  kPaytoOn,
};

}  // namespace

class ProtocolHandlerRegistrySchemeTest
    : public ProtocolHandlerRegistryTest,
      public ::testing::WithParamInterface<ProtocolTestMode> {
 public:
  ~ProtocolHandlerRegistrySchemeTest() override = default;

 private:
  void SetUp() override {
    ProtocolHandlerRegistryTest::SetUp();
    switch (GetParam()) {
      case ProtocolTestMode::kPaytoOff:
        scoped_feature_list_.InitWithFeatures(
            {}, {blink::features::kSafelistPaytoToRegisterProtocolHandler});
        break;
      case ProtocolTestMode::kPaytoOn:
        scoped_feature_list_.InitWithFeatures(
            {blink::features::kSafelistPaytoToRegisterProtocolHandler}, {});
        break;
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};
INSTANTIATE_TEST_SUITE_P(All,
                         ProtocolHandlerRegistrySchemeTest,
                         testing::Values(ProtocolTestMode::kPaytoOff,
                                         ProtocolTestMode::kPaytoOn));
// See
// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
TEST_P(ProtocolHandlerRegistrySchemeTest, SafelistedSchemes) {
  const std::string kSchemes[] = {
      "bitcoin",  "cabal",       "dat",    "did",    "doi",   "dweb",
      "ethereum", "geo",         "hyper",  "im",     "ipfs",  "ipns",
      "irc",      "ircs",        "magnet", "mailto", "mms",   "news",
      "nntp",     "openpgp4fpr", "sip",    "sms",    "smsto", "ssb",
      "ssh",      "tel",         "urn",    "webcal", "wtai",  "xmpp"};
  const std::string kFtpSchemes[] = {"ftp", "ftps", "sftp"};
  const std::string kPaytoScheme = "payto";
  for (auto& scheme : kSchemes) {
    registry()->OnAcceptRegisterProtocolHandler(
        CreateProtocolHandler(scheme, GURL("https://example.com/url=%s")));
    ASSERT_TRUE(registry()->IsHandledProtocol(scheme));
  }
  for (auto& scheme : kFtpSchemes) {
    registry()->OnAcceptRegisterProtocolHandler(
        CreateProtocolHandler(scheme, GURL("https://example.com/url=%s")));
    ASSERT_TRUE(registry()->IsHandledProtocol(scheme));
  }
  registry()->OnAcceptRegisterProtocolHandler(
    CreateProtocolHandler(kPaytoScheme, GURL("https://example.com/url=%s")));
  if (GetParam() == ProtocolTestMode::kPaytoOn) {
    ASSERT_TRUE(registry()->IsHandledProtocol(kPaytoScheme));
  } else {
    ASSERT_FALSE(registry()->IsHandledProtocol(kPaytoScheme));
  }
}

namespace {

enum class CredentialsTestMode {
  kStripCredentials,
  kKeepCredentials,
};

}  // namespace

class ProtocolHandlerRegistryCredentialsTest
    : public ProtocolHandlerRegistryTest,
      public ::testing::WithParamInterface<CredentialsTestMode> {
 public:
  ~ProtocolHandlerRegistryCredentialsTest() override = default;

 private:
  void SetUp() override {
    ProtocolHandlerRegistryTest::SetUp();
    if (GetParam() == CredentialsTestMode::kStripCredentials) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kStripCredentialsForExternalProtocolHandler);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kStripCredentialsForExternalProtocolHandler);
    }
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};
INSTANTIATE_TEST_SUITE_P(
    All,
    ProtocolHandlerRegistryCredentialsTest,
    testing::Values(CredentialsTestMode::kStripCredentials,
                    CredentialsTestMode::kKeepCredentials));

// See
// https://html.spec.whatwg.org/multipage/system-state.html#security-and-privacy
// guidance on mitigating credential leaks.
TEST_P(ProtocolHandlerRegistryCredentialsTest,
       NoCredentialsForStandardSchemes) {
  ProtocolHandler ph =
      CreateProtocolHandler("ftp", GURL("https://example.com/url=%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph);

  EXPECT_EQ(ph.TranslateUrl(GURL("ftp://example/y")),
            GURL("https://example.com/url=ftp%3A%2F%2Fexample%2Fy"));
  if (GetParam() == CredentialsTestMode::kStripCredentials) {
    EXPECT_EQ(ph.TranslateUrl(GURL("ftp://user@example/y")),
              GURL("https://example.com/url=ftp%3A%2F%2Fexample%2Fy"));
    EXPECT_EQ(ph.TranslateUrl(GURL("ftp://:password@example/y")),
              GURL("https://example.com/url=ftp%3A%2F%2Fexample%2Fy"));
    EXPECT_EQ(ph.TranslateUrl(GURL("ftp://user:password@example/y")),
              GURL("https://example.com/url=ftp%3A%2F%2Fexample%2Fy"));
    EXPECT_EQ(ph.TranslateUrl(GURL("ftp://user:password@example/y#ref")),
              GURL("https://example.com/url=ftp%3A%2F%2Fexample%2Fy%23ref"));
  } else {
    EXPECT_EQ(ph.TranslateUrl(GURL("ftp://user@example/y")),
              GURL("https://example.com/url=ftp%3A%2F%2Fuser%40example%2Fy"));
    EXPECT_EQ(
        ph.TranslateUrl(GURL("ftp://:password@example/y")),
        GURL("https://example.com/url=ftp%3A%2F%2F%3Apassword%40example%2Fy"));
    EXPECT_EQ(ph.TranslateUrl(GURL("ftp://user:password@example/y")),
              GURL("https://example.com/"
                   "url=ftp%3A%2F%2Fuser%3Apassword%40example%2Fy"));
    EXPECT_EQ(ph.TranslateUrl(GURL("ftp://user:password@example/y#ref")),
              GURL("https://example.com/"
                   "url=ftp%3A%2F%2Fuser%3Apassword%40example%2Fy%23ref"));
  }
}

TEST_F(ProtocolHandlerRegistryTest, CredentialsForNonStandardSchemes) {
  ProtocolHandler ph =
      CreateProtocolHandler("web+bool", GURL("https://example.com/url=%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph);
  EXPECT_EQ(ph.TranslateUrl(GURL("web+bool://user:password@example/y")),
            GURL("https://example.com/"
                 "url=web%2Bbool%3A%2F%2Fexample%2Fy"));
}

// ---------------------------------------------------------------------------
// OTR (Off-The-Record / Incognito) registry tests
// ---------------------------------------------------------------------------

class ProtocolHandlerRegistryOTRTest : public ProtocolHandlerRegistryTest {
 protected:
  void SetUp() override {
    ProtocolHandlerRegistryTest::SetUp();
    SetUpOTRRegistry();
  }

  void TearDown() override {
    TearDownOTRRegistry();
    ProtocolHandlerRegistryTest::TearDown();
  }

  void SetUpOTRRegistry() {
    // The OTR registry is constructed with a null PrefService, matching the
    // factory behavior for OTR browser contexts. It is isolated from the
    // parent profile's prefs and does not persist registrations.
    auto delegate = std::make_unique<TestProtocolHandlerRegistryDelegate>();
    otr_delegate_ = delegate.get();
    otr_registry_ = std::make_unique<ProtocolHandlerRegistry>(
        /*prefs=*/nullptr, std::move(delegate),
        /*is_off_the_record=*/true);
    otr_registry_->InitProtocolSettings();
  }

  void TearDownOTRRegistry() {
    otr_delegate_ = nullptr;
    otr_registry_->Shutdown();
    otr_registry_.reset();
  }

  void RecreateOTRRegistry() {
    TearDownOTRRegistry();
    SetUpOTRRegistry();
  }

  ProtocolHandlerRegistry* otr_registry() { return otr_registry_.get(); }

 private:
  raw_ptr<TestProtocolHandlerRegistryDelegate> otr_delegate_ = nullptr;
  std::unique_ptr<ProtocolHandlerRegistry> otr_registry_;
};

// Verify that the OTR profile gets a separate registry instance that is
// isolated from the regular profile's handlers; including after the OTR
// registry is re-initialized against fresh parent state.
TEST_F(ProtocolHandlerRegistryOTRTest, OTRDoesNotInheritHandlersFromRegular) {
  ProtocolHandler handler =
      CreateProtocolHandler("web+test", GURL("https://example.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_TRUE(registry()->IsHandledProtocol("web+test"));

  RecreateOTRRegistry();

  EXPECT_NE(registry(), otr_registry());
  EXPECT_FALSE(otr_registry()->IsHandledProtocol("web+test"));
  EXPECT_TRUE(otr_registry()->GetHandlersFor("web+test").empty());
}

// Verify that registering a handler in the OTR profile does not affect the
// parent profile's registry.
TEST_F(ProtocolHandlerRegistryOTRTest, OTRRegistrationDoesNotLeakToParent) {
  ASSERT_FALSE(registry()->IsHandledProtocol("web+otrtest"));
  ASSERT_FALSE(otr_registry()->IsHandledProtocol("web+otrtest"));

  ProtocolHandler handler =
      CreateProtocolHandler("web+otrtest", GURL("https://example.com/%s"));
  otr_registry()->OnAcceptRegisterProtocolHandler(handler);

  EXPECT_TRUE(otr_registry()->IsHandledProtocol("web+otrtest"));
  EXPECT_FALSE(registry()->IsHandledProtocol("web+otrtest"));
}

// Verify that OTR handlers are ephemeral and vanish when the OTR registry is
// destroyed and re-created.
TEST_F(ProtocolHandlerRegistryOTRTest, OTRHandlersDoNotPersistAcrossSessions) {
  ProtocolHandler handler =
      CreateProtocolHandler("web+ephemeral", GURL("https://example.com/%s"));
  otr_registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_TRUE(otr_registry()->IsHandledProtocol("web+ephemeral"));

  RecreateOTRRegistry();

  EXPECT_FALSE(otr_registry()->IsHandledProtocol("web+ephemeral"));
}

// OTR registrations stay in-memory on the OTR registry (it has no PrefService)
// and must never reach the regular profile's persisted prefs.
TEST_F(ProtocolHandlerRegistryOTRTest,
       OTRRegistrationDoesNotAffectParentPrefs) {
  ASSERT_TRUE(GetPrefs()
                  ->GetList(custom_handlers::prefs::kRegisteredProtocolHandlers)
                  .empty());

  ProtocolHandler otr_handler =
      CreateProtocolHandler("web+otrpref", GURL("https://otr.example.com/%s"));
  otr_registry()->OnAcceptRegisterProtocolHandler(otr_handler);

  EXPECT_EQ(1u, otr_registry()->GetHandlersFor("web+otrpref").size());
  EXPECT_TRUE(GetPrefs()
                  ->GetList(custom_handlers::prefs::kRegisteredProtocolHandlers)
                  .empty());

  // A handler registered in the regular registry does persist.
  ProtocolHandler regular_handler = CreateProtocolHandler(
      "web+regular", GURL("https://regular.example.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(regular_handler);

  EXPECT_EQ(1u,
            GetPrefs()
                ->GetList(custom_handlers::prefs::kRegisteredProtocolHandlers)
                .size());
}

// Ignoring a handler in the OTR profile must not affect the regular profile's
// ignored handler list, either at the registry or pref-store level.
TEST_F(ProtocolHandlerRegistryOTRTest, OTRIgnoredHandlerDoesNotLeakToParent) {
  ProtocolHandler handler =
      CreateProtocolHandler("web+ignored", GURL("https://example.com/%s"));

  ASSERT_TRUE(registry()->GetIgnoredHandlers().empty());
  ASSERT_TRUE(otr_registry()->GetIgnoredHandlers().empty());
  ASSERT_TRUE(GetPrefs()
                  ->GetList(custom_handlers::prefs::kIgnoredProtocolHandlers)
                  .empty());

  otr_registry()->OnIgnoreRegisterProtocolHandler(handler);

  EXPECT_TRUE(otr_registry()->IsIgnored(handler));
  EXPECT_EQ(1u, otr_registry()->GetIgnoredHandlers().size());

  EXPECT_FALSE(registry()->IsIgnored(handler));
  EXPECT_TRUE(registry()->GetIgnoredHandlers().empty());
  EXPECT_TRUE(GetPrefs()
                  ->GetList(custom_handlers::prefs::kIgnoredProtocolHandlers)
                  .empty());
}

// Disabling the OTR registry must not disable the regular profile's registry.
TEST_F(ProtocolHandlerRegistryOTRTest, OTRDisableDoesNotAffectRegularProfile) {
  ASSERT_TRUE(registry()->enabled());
  ASSERT_TRUE(otr_registry()->enabled());

  otr_registry()->Disable();

  EXPECT_FALSE(otr_registry()->enabled());
  EXPECT_TRUE(registry()->enabled());
  EXPECT_TRUE(
      GetPrefs()->GetBoolean(custom_handlers::prefs::kCustomHandlersEnabled));
}

// Setting a default handler in the OTR profile must not change the default
// handler in the regular profile, even when both register handlers for the
// same scheme.
TEST_F(ProtocolHandlerRegistryOTRTest, OTRDefaultHandlerDoesNotLeakToParent) {
  GURL regular_url("https://regular.example.com/%s");
  GURL otr_url("https://otr.example.com/%s");

  ASSERT_TRUE(registry()->GetHandlersFor("web+default").empty());
  ASSERT_TRUE(otr_registry()->GetHandlersFor("web+default").empty());

  ProtocolHandler regular_handler =
      CreateProtocolHandler("web+default", regular_url);
  registry()->OnAcceptRegisterProtocolHandler(regular_handler);

  ProtocolHandler otr_handler = CreateProtocolHandler("web+default", otr_url);
  otr_registry()->OnAcceptRegisterProtocolHandler(otr_handler);

  EXPECT_EQ(1u, registry()->GetHandlersFor("web+default").size());
  EXPECT_EQ(regular_url, registry()->GetHandlerFor("web+default").url());
  EXPECT_EQ(otr_url, otr_registry()->GetHandlerFor("web+default").url());
  EXPECT_EQ(1u,
            GetPrefs()
                ->GetList(custom_handlers::prefs::kRegisteredProtocolHandlers)
                .size());
}

TEST_F(ProtocolHandlerRegistryTest, GetHandlerForNonIncognitoReturnsHandler) {
  ProtocolHandler handler =
      CreateProtocolHandler("news", GURL("https://test.com/%s"));
  handler.set_is_allowed_in_incognito(false);
  registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_FALSE(registry()->GetHandlerFor("news").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, GetHandlerForIncognitoAllowed) {
  ProtocolHandler handler =
      CreateProtocolHandler("news", GURL("https://test.com/%s"));
  handler.set_is_allowed_in_incognito(true);
  registry()->OnAcceptRegisterProtocolHandler(handler);
  SwitchToIncognito();
  ASSERT_FALSE(registry()->GetHandlerFor("news").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest, IsHandledProtocolIncognitoDisallowed) {
  ProtocolHandler handler =
      CreateProtocolHandler("news", GURL("https://test.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_TRUE(registry()->IsHandledProtocol("news"));

  // After switching to OTR, the insertion guard rejects the pref-loaded
  // disallowed handler, so IsHandledProtocol returns false.
  SwitchToIncognito();
  ASSERT_FALSE(registry()->IsHandledProtocol("news"));
}

TEST_F(ProtocolHandlerRegistryTest, ExtensionHandlerIncognitoAllowed) {
  ProtocolHandler handler = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/true);
  registry()->OnAcceptRegisterProtocolHandler(handler);
  SwitchToIncognito();
  ASSERT_FALSE(registry()->GetHandlerFor("news").IsEmpty());
}

// Encode/decode must preserve the is_allowed_in_incognito flag so that the
// insertion guard sees the correct value when pref entries are loaded.
TEST_F(ProtocolHandlerRegistryTest, SaveLoadPreservesIsAllowedInIncognito) {
  ProtocolHandler handler = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_GT(registry()->GetHandlersFor("news").size(), 0u);

  RecreateRegistry(true);
  const auto loaded = registry()->GetHandlersFor("news");
  ASSERT_EQ(1u, loaded.size());
  EXPECT_FALSE(loaded[0].is_allowed_in_incognito());
}

TEST_F(ProtocolHandlerRegistryTest, NeedsConfirmationNoDefaultHandler) {
  // No handler registered, so GetHandlerFor returns empty.
  ASSERT_TRUE(registry()->GetHandlerFor("news").IsEmpty());
  ASSERT_FALSE(registry()->ProtocolHandlerNeedsConfirmation("news"));
}

TEST_F(ProtocolHandlerRegistryTest, NeedsConfirmationConfirmedHandler) {
  ProtocolHandler handler =
      CreateProtocolHandler("news", GURL("https://test.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(handler);
  // Handler exists and is confirmed.
  ASSERT_FALSE(registry()->GetHandlerFor("news").IsEmpty());
  ASSERT_FALSE(registry()->ProtocolHandlerNeedsConfirmation("news"));
}

TEST_F(ProtocolHandlerRegistryTest,
       NeedsConfirmationUnconfirmedExtensionHandler) {
  ProtocolHandler handler = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/true);
  registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_TRUE(registry()->ProtocolHandlerNeedsConfirmation("news"));
}

TEST_F(ProtocolHandlerRegistryTest, NeedsConfirmationIncognitoDisallowed) {
  ProtocolHandler handler = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_TRUE(registry()->ProtocolHandlerNeedsConfirmation("news"));

  // After switching to OTR, the disallowed handler is rejected at pref-load
  // and never enters storage.
  SwitchToIncognito();
  ASSERT_TRUE(registry()->GetHandlerFor("news").IsEmpty());
  ASSERT_FALSE(registry()->ProtocolHandlerNeedsConfirmation("news"));
}

TEST_F(ProtocolHandlerRegistryTest, NeedsConfirmationIncognitoAllowed) {
  ProtocolHandler handler = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/true);
  registry()->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_TRUE(registry()->ProtocolHandlerNeedsConfirmation("news"));

  SwitchToIncognito();
  ASSERT_TRUE(registry()->ProtocolHandlerNeedsConfirmation("news"));
}

// Insertion-guard tests. The OTR invariant: RegisterProtocolHandler rejects
// handlers whose is_allowed_in_incognito is false. All registration paths
// funnel through that single chokepoint, so an OTR registry cannot store a
// disallowed handler.

// Extension handler with is_allowed_in_incognito=false must not enter OTR
// storage. None of the public views of storage should surface it.
TEST_F(ProtocolHandlerRegistryTest,
       RegisterProtocolHandlerRejectsDisallowedInIncognito) {
  SwitchToIncognito();

  ProtocolHandler handler = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(handler);

  EXPECT_TRUE(registry()->GetHandlerFor("news").IsEmpty());
  EXPECT_TRUE(registry()->GetHandlersFor("news").empty());
  EXPECT_FALSE(registry()->IsRegistered(handler));
  EXPECT_FALSE(registry()->IsRegisteredByUser(handler));

  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  EXPECT_TRUE(protocols.empty());

  EXPECT_TRUE(registry()
                  ->GetUserDefinedHandlers(base::Time(), base::Time::Max())
                  .empty());
  EXPECT_TRUE(registry()->GetExtensionProtocolHandlers().empty());
}

// OnAcceptRegisterProtocolHandler must continue auto-promoting non-extension
// handlers to is_allowed_in_incognito=true in OTR mode (a user explicitly
// accepting a registration in incognito wants it usable there). This guards
// against the insertion guard turning the auto-promote into a regression.
TEST_F(ProtocolHandlerRegistryTest,
       OnAcceptInIncognitoStillAutoAllowsNonExtension) {
  SwitchToIncognito();

  ProtocolHandler handler =
      CreateProtocolHandler("news", GURL("https://test.com/%s"));
  ASSERT_FALSE(handler.is_allowed_in_incognito());
  registry()->OnAcceptRegisterProtocolHandler(handler);

  EXPECT_FALSE(registry()->GetHandlerFor("news").IsEmpty());
  EXPECT_TRUE(registry()->GetHandlerFor("news").is_allowed_in_incognito());
}

// OnDenyRegisterProtocolHandler does NOT auto-promote, so a deny in OTR with
// an unset is_allowed_in_incognito flag must end with the handler absent
// from storage (rather than silently captured by the deny path).
TEST_F(ProtocolHandlerRegistryTest,
       OnDenyInIncognitoDoesNotPersistDisallowedHandler) {
  SwitchToIncognito();

  ProtocolHandler handler =
      CreateProtocolHandler("news", GURL("https://test.com/%s"));
  ASSERT_FALSE(handler.is_allowed_in_incognito());
  registry()->OnDenyRegisterProtocolHandler(handler);

  EXPECT_FALSE(registry()->IsRegistered(handler));
  EXPECT_FALSE(registry()->IsRegisteredByUser(handler));
}

// AttemptReplace must refuse to perform destructive removals when the new
// handler isn't accessible in the current mode. Without the top-level guard,
// to-replace candidates would be removed before discovering the new handler
// couldn't be inserted, leaving the registry with strictly fewer handlers
// than it started with. Auto-promotion of a flag=false handler is only
// applied by OnAcceptRegisterProtocolHandler (explicit user acceptance);
// AttemptReplace's silent path must not fabricate that consent.
TEST_F(ProtocolHandlerRegistryTest,
       AttemptReplaceInIncognitoBailsWithoutDestructiveRemoval) {
  SwitchToIncognito();

  // Pre-register an allowed handler — OnAccept auto-promotes the flag in OTR,
  // so this lands in storage with is_allowed_in_incognito=true.
  ProtocolHandler allowed_existing =
      CreateProtocolHandler("mailto", GURL("https://test.com/%s"));
  registry()->OnAcceptRegisterProtocolHandler(allowed_existing);
  ASSERT_FALSE(registry()->GetHandlerFor("mailto").IsEmpty());

  // Construct a disallowed same-origin replacement candidate and call
  // AttemptReplace directly (no auto-promote in this path).
  ProtocolHandler disallowed_replacement =
      CreateProtocolHandler("mailto", GURL("https://test.com/updated/%s"));
  ASSERT_FALSE(disallowed_replacement.is_allowed_in_incognito());

  EXPECT_FALSE(registry()->AttemptReplace(disallowed_replacement));

  // The pre-registered handler must still be in storage — no destructive
  // removal happened.
  EXPECT_FALSE(registry()->GetHandlerFor("mailto").IsEmpty());
  EXPECT_EQ(allowed_existing.url(), registry()->GetHandlerFor("mailto").url());
}

// Mixed-operations regression net: in OTR mode, every handler returned by any
// storage-backed view must satisfy the invariant.
TEST_F(ProtocolHandlerRegistryTest, StorageInvariantHoldsAcrossMixedOps) {
  SwitchToIncognito();

  ProtocolHandler allowed_ext = CreateExtensionProtocolHandler(
      "news", GURL("https://allowed.example/%s"), "ext_a",
      /*is_allowed_in_incognito=*/true);
  registry()->OnAcceptRegisterProtocolHandler(allowed_ext);

  ProtocolHandler disallowed_ext = CreateExtensionProtocolHandler(
      "mail", GURL("https://disallowed.example/%s"), "ext_b",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(disallowed_ext);

  ProtocolHandler web_handler =
      CreateProtocolHandler("im", GURL("https://im.example/%s"));
  registry()->OnAcceptRegisterProtocolHandler(web_handler);

  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  for (const std::string& p : protocols) {
    auto handlers = registry()->GetHandlersFor(p);
    EXPECT_FALSE(handlers.empty()) << "scheme=" << p;
    for (const ProtocolHandler& h : handlers) {
      EXPECT_TRUE(h.is_allowed_in_incognito())
          << "scheme=" << p << " leaked a disallowed handler in OTR mode.";
    }
  }

  EXPECT_TRUE(registry()->GetHandlersFor("mail").empty());
  EXPECT_FALSE(registry()->IsRegistered(disallowed_ext));
  EXPECT_TRUE(registry()->GetExtensionProtocolHandlers("ext_b").empty());
}

// Pref-load path: when an OTR registry initializes from prefs (e.g. via the
// OverlayUserPrefStore reading through to regular-profile handlers), the
// insertion guard must reject disallowed entries before they enter OTR
// storage.
TEST_F(ProtocolHandlerRegistryTest, PrefLoadRespectsGuardInIncognito) {
  ProtocolHandler disallowed_ext = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(disallowed_ext);
  ASSERT_FALSE(registry()->GetHandlerFor("news").IsEmpty());
  ASSERT_GT(InPrefHandlerCount(), 0);

  // Recreate as an OTR registry sharing the same PrefService.
  // InitProtocolSettings runs the pref-load path under the insertion guard.
  SwitchToIncognito();

  EXPECT_TRUE(registry()->GetHandlerFor("news").IsEmpty());
  EXPECT_FALSE(registry()->IsRegistered(disallowed_ext));
}

// Consequence tests for read methods that iterate storage without filtering.
// Because of the insertion-time invariant, they return correct results in OTR
// without per-method filter code.

TEST_F(ProtocolHandlerRegistryTest, GetRegisteredProtocolsCleanInIncognito) {
  SwitchToIncognito();

  ProtocolHandler allowed = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_a",
      /*is_allowed_in_incognito=*/true);
  ProtocolHandler disallowed = CreateExtensionProtocolHandler(
      "mail", GURL("https://example.com/%s"), "ext_b",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(allowed);
  registry()->OnAcceptRegisterProtocolHandler(disallowed);

  std::vector<std::string> protocols;
  registry()->GetRegisteredProtocols(&protocols);
  EXPECT_EQ(std::vector<std::string>{"news"}, protocols);
}

TEST_F(ProtocolHandlerRegistryTest, GetUserDefinedHandlersCleanInIncognito) {
  SwitchToIncognito();

  ProtocolHandler allowed = CreateExtensionProtocolHandler(
      "news", GURL("https://allowed.example/%s"), "ext_a",
      /*is_allowed_in_incognito=*/true);
  ProtocolHandler disallowed = CreateExtensionProtocolHandler(
      "mail", GURL("https://disallowed.example/%s"), "ext_b",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(allowed);
  registry()->OnAcceptRegisterProtocolHandler(disallowed);

  auto handlers =
      registry()->GetUserDefinedHandlers(base::Time(), base::Time::Max());
  ASSERT_EQ(1u, handlers.size());
  EXPECT_EQ("news", handlers[0].protocol());
}

TEST_F(ProtocolHandlerRegistryTest,
       GetExtensionProtocolHandlersCleanInIncognito) {
  SwitchToIncognito();

  ProtocolHandler allowed = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_a",
      /*is_allowed_in_incognito=*/true);
  ProtocolHandler disallowed = CreateExtensionProtocolHandler(
      "mail", GURL("https://example.com/%s"), "ext_b",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(allowed);
  registry()->OnAcceptRegisterProtocolHandler(disallowed);

  auto handlers = registry()->GetExtensionProtocolHandlers();
  ASSERT_EQ(1u, handlers.size());
  ASSERT_TRUE(handlers[0].extension_id().has_value());
  EXPECT_EQ("ext_a", *handlers[0].extension_id());
}

// HasDefaultHandler / IsProtocolHandlerConfirmed use GetHandlerForInternal,
// which bypasses the runtime filter on purpose. They still return correct
// results in OTR because the insertion invariant keeps disallowed handlers
// out of the underlying storage entirely.
TEST_F(ProtocolHandlerRegistryTest, HasDefaultHandlerCleanInIncognito) {
  SwitchToIncognito();

  ProtocolHandler disallowed_ext = CreateExtensionProtocolHandler(
      "mail", GURL("https://example.com/%s"), "ext_b",
      /*is_allowed_in_incognito=*/false);
  registry()->OnAcceptRegisterProtocolHandler(disallowed_ext);

  EXPECT_FALSE(registry()->HasDefaultHandler("mail"));
  EXPECT_TRUE(registry()->GetHandlerFor("mail").IsEmpty());
}

TEST_F(ProtocolHandlerRegistryTest,
       IsProtocolHandlerConfirmedCleanInIncognito) {
  SwitchToIncognito();

  ProtocolHandler ext_handler = CreateExtensionProtocolHandler(
      "news", GURL("https://example.com/%s"), "ext_id",
      /*is_allowed_in_incognito=*/true);
  registry()->OnAcceptRegisterProtocolHandler(ext_handler);

  // Extension handlers are registered unconfirmed.
  ASSERT_TRUE(registry()->HasDefaultHandler("news"));
  EXPECT_FALSE(registry()->IsProtocolHandlerConfirmed("news"));

  registry()->ConfirmProtocolHandler("news", /*save=*/false);
  EXPECT_TRUE(registry()->IsProtocolHandlerConfirmed("news"));
}

}  // namespace custom_handlers
