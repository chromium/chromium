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
#include "url/url_features.h"

using content::BrowserThread;

namespace custom_handlers {

base::Value::Dict GetProtocolHandlerValue(const std::string& protocol,
                                          const std::string& url) {
  base::Value::Dict value;
  value.Set("protocol", protocol);
  value.Set("url", url);
  return value;
}

base::Value::Dict GetProtocolHandlerValueWithDefault(
    const std::string& protocol,
    const std::string& url,
    bool is_default) {
  base::Value::Dict value = GetProtocolHandlerValue(protocol, url);
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

  bool ProtocolHandlerCanRegisterProtocol(
      const std::string& protocol,
      const GURL& handler_url,
      blink::ProtocolHandlerSecurityLevel security_level) {
    registry()->OnAcceptRegisterProtocolHandler(
        CreateProtocolHandler(protocol, handler_url, security_level));
    return registry()->IsHandledProtocol(protocol);
  }

  void RecreateRegistry(bool initialize) {
    TeadDownRegistry();
    SetUpRegistry(initialize);
  }

  int InPrefHandlerCount() {
    const base::Value::List& in_pref_handlers = GetPrefs()->GetList(
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
    const base::Value::List& in_pref_ignored_handlers =
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
  void SetUpRegistry(bool initialize) {
    DCHECK(browser_context_);
    auto delegate = std::make_unique<TestProtocolHandlerRegistryDelegate>();
    delegate_ = delegate.get();
    registry_ = std::make_unique<ProtocolHandlerRegistry>(GetPrefs(),
                                                          std::move(delegate));
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
  ProtocolHandler handler("news", GURL("https://example.com"), "app_id", now,
                          blink::ProtocolHandlerSecurityLevel::kStrict);
  auto value = handler.Encode();
  ProtocolHandler recreated = ProtocolHandler::CreateProtocolHandler(value);
  EXPECT_EQ("news", recreated.protocol());
  EXPECT_EQ(GURL("https://example.com"), recreated.url());
  EXPECT_EQ(now, recreated.last_modified());
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

// Non-special URLs behavior is affected by the
// StandardCompliantNonSpecialSchemeURLParsing feature.
// See https://crbug.com/40063064 for details.
class ProtocolHandlerRegistryParamTest
    : public ProtocolHandlerRegistryTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ProtocolHandlerRegistryParamTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      scoped_feature_list_.InitAndEnableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    }
  }

 protected:
  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#define URL_p1u1 "https://p1u1.com/%s"
#define URL_p1u2 "https://p1u2.com/%s"
#define URL_p1u3 "https://p1u3.com/%s"
#define URL_p2u1 "https://p2u1.com/%s"
#define URL_p2u2 "https://p2u2.com/%s"
#define URL_p3u1 "https://p3u1.com/%s"

TEST_F(ProtocolHandlerRegistryTest, TestPrefPolicyOverlapRegister) {
  base::Value::List handlers_registered_by_pref;
  base::Value::List handlers_registered_by_policy;

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
  base::Value::List handlers_ignored_by_pref;
  base::Value::List handlers_ignored_by_policy;

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

TEST_P(ProtocolHandlerRegistryParamTest, TestURIPercentEncoding) {
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

  if (!use_standard_compliant_non_special_scheme_url_parsing_) {
    // We can't test a non-special URL which includes a space in the host part,
    // as such URLs become invalid when the feature is enabled.
    // TODO(crbug.com/40063064) Remove this test when the feature is shipped.
    translated_url = ph.TranslateUrl(GURL("web+custom://custom handler"));
    ASSERT_EQ(
        translated_url,
        GURL("https://test.com/url=web%2Bcustom%3A%2F%2Fcustom%20handler"));
  }

  // Query parameters.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom?foo=bar&bar=baz"));
  ASSERT_EQ(translated_url,
            GURL("https://test.com/"
                 "url=web%2Bcustom%3A%2F%2Fcustom%3Ffoo%3Dbar%26bar%3Dbaz"));

  // Non-ASCII characters.
  translated_url = ph.TranslateUrl(GURL("web+custom://custom/<>`{}#?\"'😂"));

  // When StandardCompliantNonSpecialSchemeURLParsing feature is enabled, some
  // punctuation characters in the path part of a non-special URLs are correctly
  // percent-encoded. Thus, we need to test both cases.
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // When the feature is enabled, a URL like
    // "web+custom://custom/<>`{}#?\"'😂" is correctly percent-encoded. This
    // results in the serialized URL:
    // "web+custom://custom/%3C%3E%60%7B%7D#?%22'%F0%9F%98%82"
    ASSERT_EQ(
        translated_url,
        GURL("https://test.com/"
             "url=web%2Bcustom%3A%2F%2Fcustom%2F%253C%253E%2560%257B%257D%"
             "23%3F%2522'%25F0%259F%2598%2582"));
  } else {
    // When the feature is disabled, a URL like
    // "web+custom://custom/<>`{}#?\"'😂", is not correctly percent-encoded.
    // This results in the serialized URL:
    // web+custom://custom/<>`{}#?%22'%F0%9F%98%82".
    ASSERT_EQ(translated_url,
              GURL("https://test.com/"
                   "url=web%2Bcustom%3A%2F%2Fcustom%2F%3C%3E%60%"
                   "7B%7D%23%3F%2522'%25F0%259F%2598%2582"));
  }

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

namespace {

enum class ProtocolTestMode {
  kFtpOffPaytoOn,
  kFtpOnPaytoOff,
  kFtpOnPaytoOn,
  kFtpOffPaytoOff,
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
    case ProtocolTestMode::kFtpOffPaytoOn:
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kSafelistPaytoToRegisterProtocolHandler},
          {blink::features::kSafelistFTPToRegisterProtocolHandler});
      break;
    case ProtocolTestMode::kFtpOnPaytoOff:
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kSafelistFTPToRegisterProtocolHandler},
          {blink::features::kSafelistPaytoToRegisterProtocolHandler});
      break;
    case ProtocolTestMode::kFtpOnPaytoOn:
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kSafelistFTPToRegisterProtocolHandler,
          blink::features::kSafelistPaytoToRegisterProtocolHandler}, {});
      break;
    case ProtocolTestMode::kFtpOffPaytoOff:
    default:
      scoped_feature_list_.InitWithFeatures({},
          {blink::features::kSafelistFTPToRegisterProtocolHandler,
          blink::features::kSafelistPaytoToRegisterProtocolHandler});
      break;
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};
INSTANTIATE_TEST_SUITE_P(All,
                         ProtocolHandlerRegistrySchemeTest,
                         testing::Values(ProtocolTestMode::kFtpOffPaytoOn,
                                         ProtocolTestMode::kFtpOnPaytoOff,
                                         ProtocolTestMode::kFtpOnPaytoOn,
                                         ProtocolTestMode::kFtpOffPaytoOff));
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
    if (GetParam() == ProtocolTestMode::kFtpOnPaytoOff ||
        GetParam() == ProtocolTestMode::kFtpOnPaytoOn) {
      ASSERT_TRUE(registry()->IsHandledProtocol(scheme));
    } else {
      ASSERT_FALSE(registry()->IsHandledProtocol(scheme));
    }
  }
  registry()->OnAcceptRegisterProtocolHandler(
    CreateProtocolHandler(kPaytoScheme, GURL("https://example.com/url=%s")));
  if (GetParam() == ProtocolTestMode::kFtpOffPaytoOn ||
      GetParam() == ProtocolTestMode::kFtpOnPaytoOn) {
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

TEST_P(ProtocolHandlerRegistryParamTest, CredentialsForNonStandardSchemes) {
  ProtocolHandler ph =
      CreateProtocolHandler("web+bool", GURL("https://example.com/url=%s"));
  registry()->OnAcceptRegisterProtocolHandler(ph);

  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // If the feature is enabled, the credentials part, "user:password", in
    // non-special URLs is correctly parsed and removed.
    EXPECT_EQ(ph.TranslateUrl(GURL("web+bool://user:password@example/y")),
              GURL("https://example.com/"
                   "url=web%2Bbool%3A%2F%2Fexample%2Fy"));
  } else {
    EXPECT_EQ(ph.TranslateUrl(GURL("web+bool://user:password@example/y")),
              GURL("https://example.com/"
                   "url=web%2Bbool%3A%2F%2Fuser%3Apassword%40example%2Fy"));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProtocolHandlerRegistryParamTest,
                         ::testing::Bool());

}  // namespace custom_handlers
