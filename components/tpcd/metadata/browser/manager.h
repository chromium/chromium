// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_BROWSER_MANAGER_H_
#define COMPONENTS_TPCD_METADATA_BROWSER_MANAGER_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "components/tpcd/metadata/common/manager_base.h"
#include "net/base/features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

using PatternSourcePredicate = base::RepeatingCallback<bool(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern)>;

namespace tpcd::metadata {

// TODO(b/333529481): Implement an observer pattern for the Manager class
//
// The Manager class will hold the content setting generated from any installed
// TPCD Metadata component and will make it available within the browser process
// and keep a synced copy within the network process.
//
// These content setting will be used primarily by the CookieSettings classes
// and will affect cookie access decisions.
class Manager : public common::ManagerBase, public Parser::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Used to update downstream isolated services with a fresh copy to the
    // grants.
    virtual void SetTpcdMetadataGrants(
        const ContentSettingsForOneType& grants) = 0;
    virtual PrefService& GetLocalState() = 0;
  };

  static Manager* GetInstance(Parser* parser, Delegate& delegate);
  Manager(Parser* parser, Delegate& delegate);
  virtual ~Manager();

  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  // IsAllowed checks whether the TPCD Metadata has any entry matching `url` and
  // `first_party_url`, if so returns true. `out_info` is used to collect
  // information about the matched entry to be used upstream.
  [[nodiscard]] bool IsAllowed(const GURL& url,
                               const GURL& first_party_url,
                               content_settings::SettingInfo* out_info) const;

  // GetGrants returns a copy of the TPCD Metadata in the form of
  // `ContentSettingsForOneType`.
  [[nodiscard]] ContentSettingsForOneType GetGrants() const;

  // SetGrantsForTesting calls on the private method `SetGrants()` to set the
  // TPCD Metadata grants for testing.
  void SetGrantsForTesting(const ContentSettingsForOneType& grants) {
    SetGrants(grants);
  }

  // ResetCohorts reset all cohorts for which `Parser::IsDtrpEligible()` is
  // true.
  void ResetCohorts();

  class RandGenerator {
   public:
    RandGenerator() = default;
    virtual ~RandGenerator() = default;

    RandGenerator(const RandGenerator&) = delete;
    RandGenerator& operator=(const RandGenerator&) = delete;

    virtual uint32_t Generate() const;
  };

  // SetRandGeneratorForTesting can be used at testing to set a deterministic
  // random number generator.
  void SetRandGeneratorForTesting(RandGenerator* generator) {
    rand_generator_.reset(generator);
  }

  void set_delegate_for_testing(Delegate& delegate) { delegate_ = delegate; }

 private:
  friend base::NoDestructor<Manager>;

  void SetGrants(const ContentSettingsForOneType& grants);

  // BuildGrantsWithPredicate builds TPCD Metadata grants based off of possibly
  // persisted cohorts. The `predicate` function will determine whether to
  // convey any persisted cohort for a given MetadataEntry into the final grant
  // or to reset it.
  ContentSettingsForOneType BuildGrantsWithPredicate(
      base::FunctionRef<bool(const MetadataEntry&)> predicate);

  // Parser::Observer:
  void OnMetadataReady() override;

  raw_ptr<Parser> parser_;
  raw_ref<Delegate> delegate_;
  mutable base::Lock grants_lock_;

  content_settings::HostIndexedContentSettings grants_ GUARDED_BY(grants_lock_);
  std::unique_ptr<RandGenerator> rand_generator_;
};

namespace helpers {
const char kMetadataCohortDistributionHistogram[] =
    "Navigation.TpcdMitigations.MetadataCohortDistribution";

std::string GenerateKeyHash(const MetadataEntry& metadata_entry);
void WriteCohortDistributionMetrics(
    const content_settings::mojom::TpcdMetadataCohort& cohort);
}  // namespace helpers
}  // namespace tpcd::metadata

#endif  // COMPONENTS_TPCD_METADATA_BROWSER_MANAGER_H_
