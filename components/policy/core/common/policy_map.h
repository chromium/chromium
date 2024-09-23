// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_MAP_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyMerger;

class PolicyMapTest;
FORWARD_DECLARE_TEST(PolicyMapTest, BlockedEntry);
FORWARD_DECLARE_TEST(PolicyMapTest, InvalidEntry);
FORWARD_DECLARE_TEST(PolicyMapTest, MergeFrom);

// A mapping of policy names to policy values for a given policy namespace.
class POLICY_EXPORT PolicyMap {
 public:
  // Types of messages that can be associated with policies. New types must be
  // added here in order to appear in the policy table.
  enum class MessageType { kInfo, kWarning, kError };

  // Types of conflicts that can be associated with policies. New conflict types
  // must be added here in order to appear in the policy table.
  enum class ConflictType { None, Override, Supersede };

  // Forward declare class so that it can be used in Entry.
  class EntryConflict;

  // Each policy maps to an Entry which keeps the policy value as well as other
  // relevant data about the policy.
  class POLICY_EXPORT Entry {
   public:
    PolicyLevel level = POLICY_LEVEL_RECOMMENDED;
    PolicyScope scope = POLICY_SCOPE_USER;
    // For debugging and displaying only. Set by provider delivering the policy.
    PolicySource source = POLICY_SOURCE_ENTERPRISE_DEFAULT;
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher;
    std::vector<EntryConflict> conflicts;
    // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (pointer to a
    // global), so there is no benefit to using a raw_ptr, only cost.
    // See kChromePolicyDetails in gen/components/policy/policy_constants.cc
    RAW_PTR_EXCLUSION const PolicyDetails* details = nullptr;

    Entry();
    Entry(PolicyLevel level,
          PolicyScope scope,
          PolicySource source,
          std::optional<base::Value> value,
          std::unique_ptr<ExternalDataFetcher> external_data_fetcher,
          const PolicyDetails* details = nullptr);
    ~Entry();

    Entry(Entry&&) noexcept;
    Entry& operator=(Entry&&) noexcept;

    // Returns a copy of |this|.
    Entry DeepCopy() const;

    // Retrieves the stored value if its type matches the desired type,
    // otherwise returns |nullptr|.
    const base::Value* value(base::Value::Type value_type) const;
    base::Value* value(base::Value::Type value_type);

    // Retrieves the stored value without performing type checking. Use the
    // type-checking versions above where possible.
    const base::Value* value_unsafe() const;
    base::Value* value_unsafe();

    void set_value(std::optional<base::Value> val);

    // Returns true if |this| equals |other|.
    bool Equals(const Entry& other) const;

    // Add a localized message given its l10n message ID.
    void AddMessage(MessageType type, int message_id);

    // Add a localized message given its l10n message ID and placeholder
    // args.
    void AddMessage(MessageType type,
                    int message_id,
                    std::vector<std::u16string>&& message_args);

    // Clear a message of a specific type given its l10n message ID.
    void ClearMessage(MessageType type, int message_id);

    // Adds a conflicting policy.
    void AddConflictingPolicy(Entry&& conflict);

    // Removes all the conflicts.
    void ClearConflicts();

    // Whether the policy has conflicting policies.
    bool HasConflicts();

    // Getter for |ignored_|.
    bool ignored() const;
    // Sets |ignored_| to true.
    void SetIgnored();

    // Marks the policy as blocked because it is not supported in the current
    // environment.
    void SetBlocked();

    // Marks the policy as invalid because it failed to validate against the
    // current schema.
    void SetInvalid();

    // Marks the policy as ignored because it does not share the priority of
    // its policy atomic group.
    void SetIgnoredByPolicyAtomicGroup();
    bool IsIgnoredByAtomicGroup() const;

    // Sets that the policy's value is a default value set by the policy
    // provider.
    void SetIsDefaultValue();
    bool IsDefaultValue() const;

    // Callback used to look up a localized string given its l10n message ID. It
    // should return a UTF-16 string.
    typedef base::RepeatingCallback<std::u16string(int message_id)>
        L10nLookupFunction;

    // Returns true if there is any message for |type|.
    bool HasMessage(MessageType type) const;

    // Returns localized messages as UTF-16 separated with LF characters. The
    // messages are organized according to message types (Warning, Error, etc).
    std::u16string GetLocalizedMessages(MessageType type,
                                        L10nLookupFunction lookup) const;

   private:
    std::optional<base::Value> value_;
    bool ignored_ = false;
    bool is_default_value_ = false;

    // Stores all message IDs separated by message types.
    std::map<MessageType,
             std::map<int, std::optional<std::vector<std::u16string>>>>
        message_ids_;
  };

  // Associates an Entry with a ConflictType.
  class POLICY_EXPORT EntryConflict {
   public:
    EntryConflict();
    EntryConflict(ConflictType type, Entry&& entry);
    ~EntryConflict();

    EntryConflict(EntryConflict&&) noexcept;
    EntryConflict& operator=(EntryConflict&&) noexcept;

    // Accessor methods for conflict type.
    void SetConflictType(ConflictType type);
    ConflictType conflict_type() const;

    // Accessor method for entry.
    const Entry& entry() const;

   private:
    ConflictType conflict_type_;
    Entry entry_;
  };

  typedef std::map<std::string, Entry> PolicyMapType;
  typedef PolicyMapType::const_reference const_reference;
  typedef PolicyMapType::const_iterator const_iterator;
  typedef PolicyMapType::iterator iterator;

  PolicyMap();
  PolicyMap(const PolicyMap&) = delete;
  PolicyMap& operator=(const PolicyMap&) = delete;
  PolicyMap(PolicyMap&& other) noexcept;
  PolicyMap& operator=(PolicyMap&& other) noexcept;
  ~PolicyMap();

  // Returns a weak reference to the entry currently stored for key |policy|,
  // or NULL if untrusted or not found. Ownership is retained by the PolicyMap.
  const Entry* Get(const std::string& policy) const;
  Entry* GetMutable(const std::string& policy);

  // Returns a weak reference to the value currently stored for key |policy| if
  // the value type matches the requested type, otherwise returns |nullptr| if
  // not found or there is a type mismatch. Ownership is retained by the
  // |PolicyMap|.
  const base::Value* GetValue(const std::string& policy,
                              base::Value::Type value_type) const;
  base::Value* GetMutableValue(const std::string& policy,
                               base::Value::Type value_type);

  // Returns a weak reference to the value currently stored for key |policy|
  // without performing type checking, otherwise returns |nullptr| if not found.
  // Ownership is retained by the |PolicyMap|. Use the type-checking versions
  // above where possible.
  const base::Value* GetValueUnsafe(const std::string& policy) const;
  base::Value* GetMutableValueUnsafe(const std::string& policy);

  // Returns true if the policy has a non-null value set.
  bool IsPolicySet(const std::string& policy) const;

  // Overwrites any existing information stored in the map for the key |policy|.
  // Resets the error for that policy to the empty string.
  void Set(const std::string& policy,
           PolicyLevel level,
           PolicyScope scope,
           PolicySource source,
           std::optional<base::Value> value,
           std::unique_ptr<ExternalDataFetcher> external_data_fetcher);

  void Set(const std::string& policy, Entry entry);

  // Adds a localized message with |message_id| to the map for the key |policy|
  // that should be shown to the user alongisde the value in the policy UI. This
  // should only be called for policies that are already stored in the map.
  void AddMessage(const std::string& policy, MessageType type, int message_id);

  // Adds a localized message with |message_id| and placeholder arguments
  // |message_args| to the map for the key |policy| that should be shown to the
  // user alongisde the value in the policy UI. The number of placeholders in
  // the policy string corresponding to |message_id| must be equal to the number
  // of arguments in |message_args|. This should only be called for policies
  // that are already stored in the map.
  void AddMessage(const std::string& policy,
                  MessageType type,
                  int message_id,
                  std::vector<std::u16string>&& message_args);

  // Return True if the policy is set but its value is ignored because it does
  // not share the highest priority from its atomic group. Returns False if the
  // policy is active or not set.
  bool IsPolicyIgnoredByAtomicGroup(const std::string& policy) const;

  // For all policies, overwrite the PolicySource with |source|.
  void SetSourceForAll(PolicySource source);

  // For all policies, mark them as invalid, e.g. when a required schema failed
  // to load.
  void SetAllInvalid();

  // Erase the given |policy|, if it exists in this map.
  void Erase(const std::string& policy);

  // Erase the given iterator |it|. Returns the iterator following |it| (which
  // could be `map_.end()`).
  iterator EraseIt(const_iterator it);

  // Swaps the internal representation of |this| with |other|.
  void Swap(PolicyMap* other);

  // Returns a copy of |this|.
  PolicyMap Clone() const;

  // Returns a copy of |this| that contains only the entries matching |filter|.
  PolicyMap CloneIf(
      const base::RepeatingCallback<bool(const_reference)>& filter) const;

  // Helper method used to merge entries corresponding to the same policy.
  // Setting |using_default_precedence| to true results in external factors,
  // such as the value of precedence metapolicies and user affiliation, to be
  // considered during the priority check.
  void MergePolicy(const std::string& policy_name,
                   const PolicyMap& other,
                   bool using_default_precedence);

  // Merges policies from |other| into |this|. Existing policies are only
  // overridden by those in |other| if they have a higher priority, as defined
  // by EntryHasHigherPriority(). If a policy is contained in both maps with the
  // same priority, the current value in |this| is preserved.
  void MergeFrom(const PolicyMap& other);

  // Merge the policy values that are coming from different sources.
  void MergeValues(const std::vector<PolicyMerger*>& mergers);

  // Loads the values in |policies| into this PolicyMap. All policies loaded
  // will have |level|, |scope| and |source| in their entries. Existing entries
  // are replaced.
  void LoadFrom(const base::Value::Dict& policies,
                PolicyLevel level,
                PolicyScope scope,
                PolicySource source);

  // Returns true if |lhs| has higher priority than |rhs|. The priority of the
  // fields are |level| > |PolicyPriority| for browser and |level| > |scope| >
  // |source| for OS. External factors such as metapolicy values are considered
  // by default for browser policies.
  bool EntryHasHigherPriority(const PolicyMap::Entry& lhs,
                              const PolicyMap::Entry& rhs) const;

  // Returns true if |lhs| has higher priority than |rhs|. The priority of the
  // fields are |level| > |PolicyPriority| for browser and |level| > |scope| >
  // |source| for OS. External factors such as metapolicy values and user
  // affiliation are optionally considered.
  bool EntryHasHigherPriority(const PolicyMap::Entry& lhs,
                              const PolicyMap::Entry& rhs,
                              bool using_default_precedence) const;

  // Returns True if at least one shared ID is found in the user and device
  // affiliation ID sets.
  bool IsUserAffiliated() const;

  // Populates the set containing user affiliation ID strings.
  void SetUserAffiliationIds(const base::flat_set<std::string>& user_ids);

  // Returns the set containing user affiliation ID strings.
  const base::flat_set<std::string>& GetUserAffiliationIds() const;

  // Populates the set containing device affiliation ID strings.
  void SetDeviceAffiliationIds(const base::flat_set<std::string>& device_ids);

  // Returns the set containing device affiliation ID strings.
  const base::flat_set<std::string>& GetDeviceAffiliationIds() const;

  // Returns the PolicyDetails which is generated with the yaml definition of
  // the `policy`.
  const PolicyDetails* GetPolicyDetails(const std::string& policy) const;

  // Sets the ChromePolicyDetailsCallback, which is used in
  // IsPolicyExternal(), in test environments
  void set_chrome_policy_details_callback_for_test(
      const GetChromePolicyDetailsCallback& details_callback);

  bool Equals(const PolicyMap& other) const;
  bool empty() const;
  size_t size() const;

  const_iterator begin() const;
  const_iterator end() const;
  iterator begin();
  iterator end();
  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(PolicyMapTest, BlockedEntry);
  FRIEND_TEST_ALL_PREFIXES(PolicyMapTest, InvalidEntry);
  FRIEND_TEST_ALL_PREFIXES(PolicyMapTest, MergeFrom);

  // Returns a weak reference to the entry currently stored for key |policy|,
  // or NULL if not found. Ownership is retained by the PolicyMap.
  const Entry* GetUntrusted(const std::string& policy) const;
  Entry* GetMutableUntrusted(const std::string& policy);

  // Helper function for Equals().
  static bool MapEntryEquals(const_reference& a, const_reference& b);

#if !BUILDFLAG(IS_CHROMEOS)
  // Updates the stored state of computed metapolicies.
  void UpdateStoredComputedMetapolicies();
#endif

  // Updates the stored state of user affiliation.
  void UpdateStoredUserAffiliation();

  // Returns True if the passed policy has a max_external_data_size > 0
  bool IsPolicyExternal(const std::string& policy);

  PolicyMapType map_;

  GetChromePolicyDetailsCallback details_callback_;

  // Affiliation
  bool is_user_affiliated_ = false;
  bool cloud_policy_overrides_platform_policy_ = false;
  bool cloud_user_policy_overrides_cloud_machine_policy_ = false;
  base::flat_set<std::string> user_affiliation_ids_;
  base::flat_set<std::string> device_affiliation_ids_;
};
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_MAP_H_
