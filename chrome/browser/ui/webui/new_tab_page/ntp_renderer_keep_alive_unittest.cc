// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_renderer_keep_alive.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/extension_url_overrides.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/chrome_url_overrides_handler.h"
#endif

namespace {

class TestRenderProcessHost : public content::MockRenderProcessHost {
 public:
  explicit TestRenderProcessHost(content::BrowserContext* browser_context)
      : content::MockRenderProcessHost(browser_context) {}

  void DisableRefCounts() override {
    are_ref_counts_disabled_ = true;
    content::MockRenderProcessHost::DisableRefCounts();
  }

  bool AreRefCountsDisabled() override { return are_ref_counts_disabled_; }

  void DecrementPendingReuseRefCount() override {
    CHECK(!are_ref_counts_disabled_);
    content::MockRenderProcessHost::DecrementPendingReuseRefCount();
  }

 private:
  bool are_ref_counts_disabled_ = false;
};

class InspectingMemoryConsumerRegistry
    : public base::TestMemoryConsumerRegistry {
 public:
  void OnMemoryConsumerAdded(uint32_t consumer_id,
                             std::string_view consumer_name,
                             base::MemoryConsumerTraits traits,
                             base::MemoryConsumer* consumer) override {
    if (consumer_name == "NtpRendererKeepAlive") {
      last_traits_ = traits;
    }
    base::TestMemoryConsumerRegistry::OnMemoryConsumerAdded(
        consumer_id, consumer_name, traits, consumer);
  }

  const std::optional<base::MemoryConsumerTraits>& last_traits() const {
    return last_traits_;
  }

 private:
  std::optional<base::MemoryConsumerTraits> last_traits_;
};

std::unique_ptr<TestingProfile> CreateProfileWithSearch() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      TemplateURLServiceFactory::GetInstance(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  std::unique_ptr<TestingProfile> profile = builder.Build();
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile.get());
  if (!service->loaded()) {
    base::RunLoop loop;
    base::CallbackListSubscription sub =
        service->RegisterOnLoadedCallback(loop.QuitClosure());
    service->Load();
    loop.Run();
  }
  return profile;
}

}  // namespace

class NtpRendererKeepAliveTest : public testing::Test {
 public:
  NtpRendererKeepAliveTest() {
    feature_list_.InitAndEnableFeature(ntp_features::kKeepNtpRendererAlive);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::optional<InspectingMemoryConsumerRegistry> memory_registry_{
      std::in_place};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NtpRendererKeepAliveTest, HostReplacementIdempotencyAndMultiProfile) {
  std::unique_ptr<TestingProfile> profile1 = CreateProfileWithSearch();
  TestRenderProcessHost rph1(profile1.get());
  TestRenderProcessHost rph2(profile1.get());
  ASSERT_TRUE(rph1.Init());
  ASSERT_TRUE(rph2.Init());

  NtpRendererKeepAlive::MaybePin(profile1.get(), &rph1);
  EXPECT_EQ(rph1.GetPendingReuseRefCountForTesting(), 1);
  ASSERT_TRUE(memory_registry_->last_traits().has_value());
  EXPECT_EQ(memory_registry_->last_traits()->supports_memory_limit,
            base::MemoryConsumerTraits::SupportsMemoryLimit::kNo);

  // Re-pinning the same host is a no-op.
  NtpRendererKeepAlive::MaybePin(profile1.get(), &rph1);
  EXPECT_EQ(rph1.GetPendingReuseRefCountForTesting(), 1);

  // Pinning a new host unpins the previous host first.
  NtpRendererKeepAlive::MaybePin(profile1.get(), &rph2);
  EXPECT_EQ(rph1.GetPendingReuseRefCountForTesting(), 0);
  EXPECT_EQ(rph2.GetPendingReuseRefCountForTesting(), 1);

  // A second profile pins its own host independently and releases on teardown.
  std::unique_ptr<TestingProfile> profile2 = CreateProfileWithSearch();
  auto rph3 = std::make_unique<TestRenderProcessHost>(profile2.get());
  ASSERT_TRUE(rph3->Init());
  NtpRendererKeepAlive::MaybePin(profile2.get(), rph3.get());
  EXPECT_EQ(rph3->GetPendingReuseRefCountForTesting(), 1);

  profile2->MaybeSendDestroyedNotification();
  EXPECT_EQ(rph3->GetPendingReuseRefCountForTesting(), 0);
  EXPECT_EQ(rph2.GetPendingReuseRefCountForTesting(), 1);
  rph3.reset();
  profile2.reset();
}

TEST_F(NtpRendererKeepAliveTest, LifecycleAndShutdownGuards) {
  std::unique_ptr<TestingProfile> profile = CreateProfileWithSearch();

  // Uninitialized / dead host is ignored.
  TestRenderProcessHost uninit_rph(profile.get());
  NtpRendererKeepAlive::MaybePin(profile.get(), &uninit_rph);
  EXPECT_EQ(uninit_rph.GetPendingReuseRefCountForTesting(), 0);

  // Deleting-soon host is ignored.
  TestRenderProcessHost deleting_rph(profile.get());
  ASSERT_TRUE(deleting_rph.Init());
  deleting_rph.Cleanup();
  ASSERT_TRUE(deleting_rph.IsDeletingSoon());
  NtpRendererKeepAlive::MaybePin(profile.get(), &deleting_rph);
  EXPECT_EQ(deleting_rph.GetPendingReuseRefCountForTesting(), 0);

  // Disabled-refcount host is ignored, and `DisableRefCounts()` while pinned is
  // handled without decrementing a disabled refcount on `Release()`.
  TestRenderProcessHost disabled_rph(profile.get());
  ASSERT_TRUE(disabled_rph.Init());
  disabled_rph.DisableRefCounts();
  NtpRendererKeepAlive::MaybePin(profile.get(), &disabled_rph);
  EXPECT_EQ(disabled_rph.GetPendingReuseRefCountForTesting(), 0);

  TestRenderProcessHost pinned_rph(profile.get());
  ASSERT_TRUE(pinned_rph.Init());
  NtpRendererKeepAlive::MaybePin(profile.get(), &pinned_rph);
  ASSERT_EQ(pinned_rph.GetPendingReuseRefCountForTesting(), 1);
  pinned_rph.DisableRefCounts();
  memory_registry_->NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::ModeratePressureThreshold());
  memory_registry_->NotifyUpdateMemoryLimit(base::MemoryLimit::Default());

  // Once profile shutdown starts, `MaybePin()` returns early without pinning.
  std::unique_ptr<TestingProfile> shutting_down_profile =
      CreateProfileWithSearch();
  TestRenderProcessHost late_rph(shutting_down_profile.get());
  ASSERT_TRUE(late_rph.Init());
  shutting_down_profile->NotifyWillBeDestroyed();
  ASSERT_TRUE(shutting_down_profile->ShutdownStarted());
  NtpRendererKeepAlive::MaybePin(shutting_down_profile.get(), &late_rph);
  EXPECT_EQ(late_rph.GetPendingReuseRefCountForTesting(), 0);

  // Destroying the global `MemoryConsumerRegistry` before `profile` does not
  // CHECK-fail (`CheckUnregister::kDisabled`).
  memory_registry_.reset();
}

TEST_F(NtpRendererKeepAliveTest, MemoryPressureBoundaries) {
  std::unique_ptr<TestingProfile> profile = CreateProfileWithSearch();
  TestRenderProcessHost rph(profile.get());
  ASSERT_TRUE(rph.Init());

  // Initial NTP load while already under moderate pressure does not pin.
  memory_registry_->NotifyUpdateMemoryLimit(
      base::MemoryLimit::ModeratePressureThreshold());
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);

  // Restoring normal limit allows pinning.
  memory_registry_->NotifyUpdateMemoryLimit(base::MemoryLimit::Default());
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 1);

  // Release notification above `ModeratePressureThreshold()` retains the pin.
  memory_registry_->NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::FromPercent(75));
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 1);

  // Moderate pressure releases the pin and blocks re-pinning until cleared.
  memory_registry_->NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::ModeratePressureThreshold());
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);

  // Raising the limit alone does not eagerly re-pin; the next load does.
  memory_registry_->NotifyUpdateMemoryLimit(base::MemoryLimit::Default());
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 1);

  // Critical pressure also releases the pin.
  memory_registry_->NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::CriticalPressureThreshold());
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);
}

TEST_F(NtpRendererKeepAliveTest, PolicyDseAndExtensionNtpOverrides) {
  std::unique_ptr<TestingProfile> profile = CreateProfileWithSearch();
  TestRenderProcessHost rph(profile.get());
  ASSERT_TRUE(rph.Init());

  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  ASSERT_EQ(rph.GetPendingReuseRefCountForTesting(), 1);

  // A non-NTP `NewTabPageLocation` policy override unpins and blocks pinning.
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kNewTabPageLocationOverride,
                   "https://example.com/ntp");
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);

  // Switching DSE away from Google also blocks pinning once policy is cleared,
  // unless `NewTabPageLocation` explicitly forces `chrome://new-tab-page/`.
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile.get());
  const TemplateURL* google_dse = service->GetDefaultSearchProvider();
  ASSERT_NE(google_dse, nullptr);
  const std::string google_guid = google_dse->sync_guid();

  TemplateURLData third_party_data;
  third_party_data.SetShortName(u"Example");
  third_party_data.SetKeyword(u"example.com");
  third_party_data.SetURL("https://example.com/search?q={searchTerms}");
  third_party_data.new_tab_url = "https://example.com/newtab";
  TemplateURL* third_party_dse =
      service->Add(std::make_unique<TemplateURL>(third_party_data));
  service->SetUserSelectedDefaultSearchProvider(third_party_dse);

  prefs->SetString(prefs::kNewTabPageLocationOverride,
                   "chrome://new-tab-page/");
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 1);

  // Clearing the policy exposes the active 3P DSE and immediately unpins.
  prefs->ClearPref(prefs::kNewTabPageLocationOverride);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);

  // Restoring Google DSE allows the next NTP load to pin again.
  TemplateURL* restored_google_dse =
      service->GetTemplateURLForGUID(google_guid);
  ASSERT_NE(restored_google_dse, nullptr);
  service->SetUserSelectedDefaultSearchProvider(restored_google_dse);
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 1);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Activating a `newtab` extension override releases the pin and blocks
  // re-pinning until removed.
  scoped_refptr<const extensions::Extension> ext =
      extensions::ExtensionBuilder("NTP Override").Build();
  extensions::ExtensionRegistry::Get(profile.get())->AddEnabled(ext);
  extensions::URLOverrides::URLOverrideMap overrides;
  overrides["newtab"] = ext->GetResourceURL("ntp.html");
  ExtensionUrlOverrides::RegisterOrActivateChromeURLOverrides(profile.get(),
                                                              overrides);

  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 0);

  ExtensionUrlOverrides::UnregisterChromeURLOverrides(profile.get(), overrides);
  NtpRendererKeepAlive::MaybePin(profile.get(), &rph);
  EXPECT_EQ(rph.GetPendingReuseRefCountForTesting(), 1);
#endif
}
