// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLACKLIST_OPT_OUT_BLACKLIST_OPT_OUT_BLACKLIST_H_
#define COMPONENTS_BLACKLIST_OPT_OUT_BLACKLIST_OPT_OUT_BLACKLIST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"

namespace base {
class Clock;
}

namespace blacklist {

class BlacklistData;
class OptOutBlacklistDelegate;
class OptOutStore;

class OptOutBlacklist {
 public:
  // |opt_out_store| is the backing store to retrieve and store blacklist
  // information, and can be null. When |opt_out_store| is null, the in-memory
  // data will be immediately loaded to empty. If |opt_out_store| is non-null,
  // it will be used to load the in-memory map asynchronously.
  // |blacklist_delegate| is a single object listening for blacklist events, and
  // it is guaranteed to outlive the life time of |this|.
  OptOutBlacklist(std::unique_ptr<OptOutStore> opt_out_store,
                  base::Clock* clock,
                  OptOutBlacklistDelegate* blacklist_delegate);
  virtual ~OptOutBlacklist();

  // Creates the BlacklistData that backs the blacklist.
  void Init();

  // Asynchronously deletes all entries in the in-memory blacklist. Informs
  // the backing store to delete entries between |begin_time| and |end_time|,
  // and reloads entries into memory from the backing store. If the embedder
  // passed in a null store, resets all history in the in-memory blacklist.
  void ClearBlackList(base::Time begin_time, base::Time end_time);

  // Asynchronously adds a new navigation to to the in-memory blacklist and
  // backing store. |opt_out| is whether the user opted out of the action. If
  // the in memory map has reached the max number of hosts allowed, and
  // |host_name| is a new host, a host will be evicted based on recency of the
  // hosts most recent opt out. It returns the time used for recording the
  // moment when the navigation is added for logging.
  base::Time AddEntry(const std::string& host_name, bool opt_out, int type);

  // Synchronously determines if the action should be allowed for |host_name|
  // and |type|. Returns the reason the blacklist disallowed the action, or
  // kAllowed if the action is allowed. Record checked reasons in
  // |passed_reasons|. |ignore_long_term_black_list_rules| will cause session,
  // type, and host rules, but the session rule will still be queried.
  BlacklistReason IsLoadedAndAllowed(
      const std::string& host_name,
      int type,
      bool ignore_long_term_black_list_rules,
      std::vector<BlacklistReason>* passed_reasons) const;

 protected:
  // Whether the session rule should be enabled. |duration| specifies how long a
  // user remains blacklisted. |history| specifies how many entries should be
  // evaluated; |threshold| specifies how many opt outs would cause
  // blacklisting. I.e., the most recent |history| are looked at and if
  // |threshold| (or more) of them are opt outs, the user is considered
  // blacklisted unless the most recent opt out was longer than |duration| ago.
  // This rule only considers entries within this session (it does not use the
  // data that was persisted in previous sessions). When the blacklist is
  // cleared, this rule is reset as if it were a new session. Queried in Init().
  virtual bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                                      size_t* history,
                                      int* threshold) const = 0;

  // Whether the persistent rule should be enabled. |duration| specifies how
  // long a user remains blacklisted. |history| specifies how many entries
  // should be evaluated; |threshold| specifies how many opt outs would cause
  // blacklisting. I.e., the most recent |history| are looked at and if
  // |threshold| (or more) of them are opt outs, the user is considered
  // blacklisted unless the most recent opt out was longer than |duration| ago.
  // Queried in Init().
  virtual bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                         size_t* history,
                                         int* threshold) const = 0;

  // Whether the host rule should be enabled. |duration| specifies how long a
  // host remains blacklisted. |history| specifies how many entries should be
  // evaluated per host; |threshold| specifies how many opt outs would cause
  // blacklisting. I.e., the most recent |history| entries per host are looked
  // at and if |threshold| (or more) of them are opt outs, the host is
  // considered blacklisted unless the most recent opt out was longer than
  // |duration| ago. |max_hosts| will limit the number of hosts stored in this
  // class when non-zero.
  // Queried in Init().
  virtual bool ShouldUseHostPolicy(base::TimeDelta* duration,
                                   size_t* history,
                                   int* threshold,
                                   size_t* max_hosts) const = 0;

  // Whether the type rule should be enabled.  |duration| specifies how long a
  // type remains blacklisted. |history| specifies how many entries should be
  // evaluated per type; |threshold| specifies how many opt outs would cause
  // blacklisting.
  // I.e., the most recent |history| entries per type are looked at and if
  // |threshold| (or more) of them are opt outs, the type is considered
  // blacklisted unless the most recent opt out was longer than |duration| ago.
  // Queried in Init().
  virtual bool ShouldUseTypePolicy(base::TimeDelta* duration,
                                   size_t* history,
                                   int* threshold) const = 0;

  // The allowed types and what version they are. Should be empty unless the
  // caller will not be using the blacklist in the session. It is used to remove
  // stale entries from the database and to DCHECK that other methods are not
  // using disallowed types. Queried in Init().
  virtual BlacklistData::AllowedTypesAndVersions GetAllowedTypes() const = 0;

 private:
  // Synchronous version of AddEntry method. |time| is the time
  // stamp of when the navigation was determined to be an opt-out or non-opt
  // out.
  void AddEntrySync(const std::string& host_name,
                    bool opt_out,
                    int type,
                    base::Time time);

  // Synchronous version of ClearBlackList method.
  void ClearBlackListSync(base::Time begin_time, base::Time end_time);

  // Callback passed to the backing store when loading black list information.
  // Takes ownership of |blacklist_data|.
  void LoadBlackListDone(std::unique_ptr<BlacklistData> blacklist_data);

  // Called while waiting for the blacklist to be loaded from the backing
  // store.
  // Enqueues a task to run when when loading blacklist information has
  // completed. Maintains the order that tasks were called in.
  void QueuePendingTask(base::OnceClosure callback);

  // An in-memory representation of the various rules of the blacklist. This is
  // null while reading from the backing store.
  std::unique_ptr<BlacklistData> blacklist_data_;

  // Whether the blacklist is done being loaded from the backing store.
  bool loaded_;

  // The backing store of the blacklist information.
  std::unique_ptr<OptOutStore> opt_out_store_;

  // Callbacks to be run after loading information from the backing store has
  // completed.
  base::queue<base::OnceClosure> pending_callbacks_;

  base::Clock* clock_;

  // The delegate listening to this blacklist. |blacklist_delegate_| lifetime is
  // guaranteed to outlive |this|.
  OptOutBlacklistDelegate* blacklist_delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OptOutBlacklist> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OptOutBlacklist);
};

}  // namespace blacklist

#endif  // COMPONENTS_BLACKLIST_OPT_OUT_BLACKLIST_OPT_OUT_BLACKLIST_H_
