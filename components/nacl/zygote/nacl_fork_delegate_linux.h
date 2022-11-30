// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_ZYGOTE_NACL_FORK_DELEGATE_LINUX_H_
#define COMPONENTS_NACL_ZYGOTE_NACL_FORK_DELEGATE_LINUX_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "content/public/common/zygote/zygote_fork_delegate_linux.h"

namespace base {
struct LaunchOptions;
}

namespace nacl {

// Appends any ZygoteForkDelegate instances needed by NaCl to |*delegates|.
void AddNaClZygoteForkDelegates(
    std::vector<std::unique_ptr<content::ZygoteForkDelegate>>* delegates);

// The NaClForkDelegate is created during Chrome linux zygote initialization,
// and provides "fork()" functionality with NaCl specific process
// characteristics (specifically address space layout) as an alternative to
// forking the zygote. A new delegate is passed in as an argument to
// ZygoteMain().
class NaClForkDelegate : public content::ZygoteForkDelegate {
 public:
  NaClForkDelegate();
  NaClForkDelegate(const NaClForkDelegate&) = delete;
  NaClForkDelegate& operator=(const NaClForkDelegate&) = delete;
  ~NaClForkDelegate() override;

  void Init(int sandboxdesc, bool enable_layer1_sandbox) override;
  void InitialUMA(std::string* uma_name,
                  int* uma_sample,
                  int* uma_boundary_value) override;
  bool CanHelp(const std::string& process_type,
               std::string* uma_name,
               int* uma_sample,
               int* uma_boundary_value) override;
  pid_t Fork(const std::string& process_type,
             const std::vector<std::string>& args,
             const std::vector<int>& fds,
             const std::string& channel_id) override;
  bool GetTerminationStatus(pid_t pid,
                            bool known_dead,
                            base::TerminationStatus* status,
                            int* exit_code) override;

 private:
  static void AddPassthroughEnvToOptions(base::LaunchOptions* options);

  // These values are reported via UMA and hence they become permanent
  // constants.  Old values cannot be reused, only new ones added.
  enum NaClHelperStatus {
    kNaClHelperUnused = 0,
    kNaClHelperMissing = 1,
    kNaClHelperBootstrapMissing = 2,
    // kNaClHelperValgrind = 3,  // Running in valgrind no longer supported.
    kNaClHelperLaunchFailed = 4,
    kNaClHelperAckFailed = 5,
    kNaClHelperSuccess = 6,
    kNaClHelperStatusBoundary  // Must be one greater than highest value used.
  };

  NaClHelperStatus status_;
  int fd_;

  FRIEND_TEST_ALL_PREFIXES(NaClForkDelegateLinuxTest, EnvPassthrough);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_ZYGOTE_NACL_FORK_DELEGATE_LINUX_H_
