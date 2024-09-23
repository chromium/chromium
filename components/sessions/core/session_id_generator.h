// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_ID_GENERATOR_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_ID_GENERATOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"

class PrefRegistrySimple;
class PrefService;

namespace sessions {

class SESSIONS_EXPORT SessionIdGenerator {
 public:
  // Returns the singleton instance of this class.
  static SessionIdGenerator* GetInstance();

  SessionIdGenerator(const SessionIdGenerator&) = delete;
  SessionIdGenerator& operator=(const SessionIdGenerator&) = delete;

  // Register preferences used by this class.
  static void RegisterPrefs(PrefRegistrySimple* prefs);

  // Initialization of the singleton. Must be called exactly once. |local_state|
  // must not be null and must exist until Shutdown() is called.
  void Init(PrefService* local_state);
  void Shutdown();

  // Creates a new instance representing an ID that has never been used before
  // locally (even across browser restarts). Must be preceded by Init().
  SessionID NewUnique();

  // Calls this method after restoring a session passing the
  // `highest_restored_id` to ensure that the next ID will not conflict with the
  // existing ones.
  void SetHighestRestoredID(SessionID highest_restored_id);

  // Preference name used to persist the last ID.
  static std::string GetLastValuePrefNameForTest();

  // Internal random function injection for tests.
  using RandomGenerator = base::RepeatingCallback<SessionID::id_type()>;
  void SetRandomGeneratorForTest(const RandomGenerator& rand_generator);

  // Used for test only, verify the SessionIdGenerator is initialized.
  bool IsInitializedForTest() const;

 private:
  friend struct base::DefaultSingletonTraits<SessionIdGenerator>;

  SessionIdGenerator();
  ~SessionIdGenerator();

  void IncrementValueBy(int increment);

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<PrefService> local_state_;
  SessionID::id_type last_value_;

  // Used to override the random number generator for tests.
  RandomGenerator rand_generator_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_ID_GENERATOR_H_
