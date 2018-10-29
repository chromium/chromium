// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/non_blocking_invalidator.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/model_type.h"
#include "components/sync/tools/null_invalidation_state_tracker.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/url_request/url_request_test_util.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

// This is a simple utility that initializes a sync notifier and
// listens to any received notifications.

namespace syncer {
namespace {

const char kEmailSwitch[] = "email";
const char kTokenSwitch[] = "token";
const char kHostPortSwitch[] = "host-port";
const char kTrySslTcpFirstSwitch[] = "try-ssltcp-first";
const char kAllowInsecureConnectionSwitch[] = "allow-insecure-connection";

// Class to print received notifications events.
class NotificationPrinter : public InvalidationHandler {
 public:
  NotificationPrinter() {}
  ~NotificationPrinter() override {}

  void OnInvalidatorStateChange(InvalidatorState state) override {
    LOG(INFO) << "Invalidator state changed to "
              << InvalidatorStateToString(state);
  }

  void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) override {
    ObjectIdSet ids = invalidation_map.GetObjectIds();
    for (auto it = ids.begin(); it != ids.end(); ++it) {
      LOG(INFO) << "Remote invalidation: " << invalidation_map.ToString();
    }
  }

  std::string GetOwnerName() const override { return "NotificationPrinter"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPrinter);
};

// Needed to use a real host resolver.
class MyTestURLRequestContext : public net::TestURLRequestContext {
 public:
  MyTestURLRequestContext() : TestURLRequestContext(true) {
    context_storage_.set_host_resolver(
        net::HostResolver::CreateDefaultResolver(nullptr));
    context_storage_.set_transport_security_state(
        std::make_unique<net::TransportSecurityState>());
    Init();
  }

  ~MyTestURLRequestContext() override {}
};

class MyTestURLRequestContextGetter : public net::TestURLRequestContextGetter {
 public:
  explicit MyTestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : TestURLRequestContextGetter(io_task_runner) {}

  net::TestURLRequestContext* GetURLRequestContext() override {
    // Construct |context_| lazily so it gets constructed on the right
    // thread (the IO thread).
    if (!context_)
      context_ = std::make_unique<MyTestURLRequestContext>();
    return context_.get();
  }

 private:
  ~MyTestURLRequestContextGetter() override {}

  std::unique_ptr<MyTestURLRequestContext> context_;
};

notifier::NotifierOptions ParseNotifierOptions(
    const base::CommandLine& command_line,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter) {
  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter = request_context_getter;

  if (command_line.HasSwitch(kHostPortSwitch)) {
    notifier_options.xmpp_host_port = net::HostPortPair::FromString(
        command_line.GetSwitchValueASCII(kHostPortSwitch));
    LOG(INFO) << "Using " << notifier_options.xmpp_host_port.ToString()
              << " for test sync notification server.";
  }

  notifier_options.try_ssltcp_first =
      command_line.HasSwitch(kTrySslTcpFirstSwitch);
  LOG_IF(INFO, notifier_options.try_ssltcp_first)
      << "Trying SSL/TCP port before XMPP port for notifications.";

  notifier_options.allow_insecure_connection =
      command_line.HasSwitch(kAllowInsecureConnectionSwitch);
  LOG_IF(INFO, notifier_options.allow_insecure_connection)
      << "Allowing insecure XMPP connections.";

  return notifier_options;
}

int SyncListenNotificationsMain(int argc, char* argv[]) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  base::MessageLoop ui_loop;
  base::Thread io_thread("IO thread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Parse command line.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string email = command_line.GetSwitchValueASCII(kEmailSwitch);
  std::string token = command_line.GetSwitchValueASCII(kTokenSwitch);
  // TODO(akalin): Write a wrapper script that gets a token for an
  // email and password and passes that in to this utility.
  if (email.empty() || token.empty()) {
    std::printf(
        "Usage: %s --%s=foo@bar.com --%s=token\n"
        "[--%s=host:port] [--%s] [--%s]\n"
        "Run chrome and set a breakpoint on\n"
        "SyncManagerImpl::UpdateCredentials() "
        "after logging into\n"
        "sync to get the token to pass into this utility.\n",
        argv[0], kEmailSwitch, kTokenSwitch, kHostPortSwitch,
        kTrySslTcpFirstSwitch, kAllowInsecureConnectionSwitch);
    return -1;
  }

  // Set up objects that monitor the network.
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::Create());

  const notifier::NotifierOptions& notifier_options = ParseNotifierOptions(
      command_line, new MyTestURLRequestContextGetter(io_thread.task_runner()));
  NetworkChannelCreator network_channel_creator =
      NonBlockingInvalidator::MakePushClientChannelCreator(notifier_options);
  const char kClientInfo[] = "sync_listen_notifications";
  NullInvalidationStateTracker null_invalidation_state_tracker;
  std::unique_ptr<Invalidator> invalidator(new NonBlockingInvalidator(
      network_channel_creator, base::RandBytesAsString(8),
      null_invalidation_state_tracker.GetSavedInvalidations(),
      null_invalidation_state_tracker.GetBootstrapData(),
      &null_invalidation_state_tracker, kClientInfo,
      notifier_options.request_context_getter));

  NotificationPrinter notification_printer;

  invalidator->UpdateCredentials(email, token);

  // Listen for notifications for all known types.
  invalidator->RegisterHandler(&notification_printer);
  bool success = invalidator->UpdateRegisteredIds(
      &notification_printer, ModelTypeSetToObjectIdSet(ModelTypeSet::All()));
  DCHECK(success);

  base::RunLoop().Run();

  invalidator->UnregisterHandler(&notification_printer);
  io_thread.Stop();
  return 0;
}

}  // namespace
}  // namespace syncer

int main(int argc, char* argv[]) {
  return syncer::SyncListenNotificationsMain(argc, argv);
}
