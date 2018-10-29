// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/invalidation/impl/non_blocking_invalidator.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/fake_encryptor.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/js/js_event_handler.h"
#include "components/sync/syncable/base_node.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/tools/null_invalidation_state_tracker.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/base/host_port_pair.h"
#include "net/dns/host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

// This is a simple utility that initializes a sync client and
// prints out any events.

// TODO(akalin): Refactor to combine shared code with
// sync_listen_notifications.
namespace syncer {
namespace {

const char kEmailSwitch[] = "email";
const char kTokenSwitch[] = "token";
const char kXmppHostPortSwitch[] = "xmpp-host-port";
const char kXmppTrySslTcpFirstSwitch[] = "xmpp-try-ssltcp-first";
const char kXmppAllowInsecureConnectionSwitch[] =
    "xmpp-allow-insecure-connection";
const char kSyncServiceURL[] = "https://clients4.google.com/chrome-sync/dev";

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

// TODO(akalin): Use system encryptor once it's moved to sync/.
class NullEncryptor : public Encryptor {
 public:
  ~NullEncryptor() override {}

  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override {
    *ciphertext = plaintext;
    return true;
  }

  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override {
    *plaintext = ciphertext;
    return true;
  }
};

std::string ValueToString(const base::Value& value) {
  std::string str;
  base::JSONWriter::Write(value, &str);
  return str;
}

class LoggingChangeDelegate : public SyncManager::ChangeDelegate {
 public:
  ~LoggingChangeDelegate() override {}

  void OnChangesApplied(ModelType model_type,
                        int64_t model_version,
                        const BaseTransaction* trans,
                        const ImmutableChangeRecordList& changes) override {
    LOG(INFO) << "Changes applied for " << ModelTypeToString(model_type);
    size_t i = 1;
    size_t change_count = changes.Get().size();
    for (auto it = changes.Get().begin(); it != changes.Get().end(); ++it) {
      std::unique_ptr<base::DictionaryValue> change_value(it->ToValue());
      LOG(INFO) << "Change (" << i << "/" << change_count
                << "): " << ValueToString(*change_value);
      if (it->action != ChangeRecord::ACTION_DELETE) {
        ReadNode node(trans);
        DCHECK_EQ(node.InitByIdLookup(it->id), BaseNode::INIT_OK);
        std::unique_ptr<base::DictionaryValue> details(node.ToValue());
        VLOG(1) << "Details: " << ValueToString(*details);
      }
      ++i;
    }
  }

  void OnChangesComplete(ModelType model_type) override {
    LOG(INFO) << "Changes complete for " << ModelTypeToString(model_type);
  }
};

class LoggingUnrecoverableErrorHandler : public UnrecoverableErrorHandler {
 public:
  ~LoggingUnrecoverableErrorHandler() override {}

  void OnUnrecoverableError(const base::Location& from_here,
                            const std::string& message) override {
    if (LOG_IS_ON(ERROR)) {
      logging::LogMessage(from_here.file_name(), from_here.line_number(),
                          logging::LOG_ERROR)
              .stream()
          << message;
    }
  }
};

class LoggingJsEventHandler
    : public JsEventHandler,
      public base::SupportsWeakPtr<LoggingJsEventHandler> {
 public:
  ~LoggingJsEventHandler() override {}

  void HandleJsEvent(const std::string& name,
                     const JsEventDetails& details) override {
    VLOG(1) << name << ": " << details.ToString();
  }
};

class InvalidationAdapter : public InvalidationInterface {
 public:
  explicit InvalidationAdapter(const Invalidation& invalidation)
      : invalidation_(invalidation) {}
  ~InvalidationAdapter() override {}

  bool IsUnknownVersion() const override {
    return invalidation_.is_unknown_version();
  }

  const std::string& GetPayload() const override {
    return invalidation_.payload();
  }

  int64_t GetVersion() const override { return invalidation_.version(); }

  void Acknowledge() override { invalidation_.Acknowledge(); }

  void Drop() override { invalidation_.Drop(); }

 private:
  Invalidation invalidation_;
};

class InvalidatorShim : public InvalidationHandler {
 public:
  explicit InvalidatorShim(SyncManager* sync_manager)
      : sync_manager_(sync_manager) {}

  void OnInvalidatorStateChange(InvalidatorState state) override {
    sync_manager_->SetInvalidatorEnabled(state == INVALIDATIONS_ENABLED);
  }

  void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) override {
    ObjectIdSet ids = invalidation_map.GetObjectIds();
    for (auto ids_it = ids.begin(); ids_it != ids.end(); ++ids_it) {
      ModelType type;
      if (!NotificationTypeToRealModelType(ids_it->name(), &type)) {
        DLOG(WARNING) << "Notification has invalid id: "
                      << ObjectIdToString(*ids_it);
      } else {
        SingleObjectInvalidationSet invalidation_set =
            invalidation_map.ForObject(*ids_it);
        for (auto inv_it = invalidation_set.begin();
             inv_it != invalidation_set.end(); ++inv_it) {
          std::unique_ptr<InvalidationInterface> inv_adapter(
              new InvalidationAdapter(*inv_it));
          sync_manager_->OnIncomingInvalidation(type, std::move(inv_adapter));
        }
      }
    }
  }

  std::string GetOwnerName() const override { return "InvalidatorShim"; }

 private:
  SyncManager* sync_manager_;
};

void LogUnrecoverableErrorContext() {
  base::debug::StackTrace().Print();
}

notifier::NotifierOptions ParseNotifierOptions(
    const base::CommandLine& command_line,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter) {
  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter = request_context_getter;
  notifier_options.auth_mechanism = "X-OAUTH2";

  if (command_line.HasSwitch(kXmppHostPortSwitch)) {
    notifier_options.xmpp_host_port = net::HostPortPair::FromString(
        command_line.GetSwitchValueASCII(kXmppHostPortSwitch));
    LOG(INFO) << "Using " << notifier_options.xmpp_host_port.ToString()
              << " for test sync notification server.";
  }

  notifier_options.try_ssltcp_first =
      command_line.HasSwitch(kXmppTrySslTcpFirstSwitch);
  LOG_IF(INFO, notifier_options.try_ssltcp_first)
      << "Trying SSL/TCP port before XMPP port for notifications.";

  notifier_options.allow_insecure_connection =
      command_line.HasSwitch(kXmppAllowInsecureConnectionSwitch);
  LOG_IF(INFO, notifier_options.allow_insecure_connection)
      << "Allowing insecure XMPP connections.";

  return notifier_options;
}

void StubNetworkTimeUpdateCallback(const base::Time&,
                                   const base::TimeDelta&,
                                   const base::TimeDelta&) {}

int SyncClientMain(int argc, char* argv[]) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  base::MessageLoop sync_loop;
  base::Thread io_thread("IO thread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Parse command line.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  SyncCredentials credentials;
  credentials.account_id = command_line.GetSwitchValueASCII(kEmailSwitch);
  credentials.email = command_line.GetSwitchValueASCII(kEmailSwitch);
  credentials.sync_token = command_line.GetSwitchValueASCII(kTokenSwitch);
  // TODO(akalin): Write a wrapper script that gets a token for an
  // email and password and passes that in to this utility.
  if (credentials.email.empty() || credentials.sync_token.empty()) {
    std::printf(
        "Usage: %s --%s=foo@bar.com --%s=token\n"
        "[--%s=host:port] [--%s] [--%s]\n"
        "Run chrome and set a breakpoint on\n"
        "SyncManagerImpl::UpdateCredentials() "
        "after logging into\n"
        "sync to get the token to pass into this utility.\n",
        argv[0], kEmailSwitch, kTokenSwitch, kXmppHostPortSwitch,
        kXmppTrySslTcpFirstSwitch, kXmppAllowInsecureConnectionSwitch);
    return -1;
  }

  // Set up sync notifier factory.
  const scoped_refptr<MyTestURLRequestContextGetter> context_getter =
      new MyTestURLRequestContextGetter(io_thread.task_runner());
  const notifier::NotifierOptions& notifier_options =
      ParseNotifierOptions(command_line, context_getter);
  NetworkChannelCreator network_channel_creator =
      NonBlockingInvalidator::MakePushClientChannelCreator(notifier_options);
  const char kClientInfo[] = "standalone_sync_client";
  std::string invalidator_id = base::RandBytesAsString(8);
  NullInvalidationStateTracker null_invalidation_state_tracker;
  std::unique_ptr<Invalidator> invalidator(new NonBlockingInvalidator(
      network_channel_creator, invalidator_id,
      null_invalidation_state_tracker.GetSavedInvalidations(),
      null_invalidation_state_tracker.GetBootstrapData(),
      &null_invalidation_state_tracker, kClientInfo,
      notifier_options.request_context_getter));

  // Set up database directory for the syncer.
  base::ScopedTempDir database_dir;
  bool success = database_dir.CreateUniqueTempDir();
  DCHECK(success);

  // Developers often add types to ModelTypeSet::All() before the server
  // supports them.  We need to be explicit about which types we want here.
  ModelTypeSet model_types;
  model_types.Put(BOOKMARKS);
  model_types.Put(PREFERENCES);
  model_types.Put(PASSWORDS);
  model_types.Put(AUTOFILL);
  model_types.Put(THEMES);
  model_types.Put(TYPED_URLS);
  model_types.Put(EXTENSIONS);
  model_types.Put(NIGORI);
  model_types.Put(SEARCH_ENGINES);
  model_types.Put(SESSIONS);
  model_types.Put(APPS);
  model_types.Put(AUTOFILL_PROFILE);
  model_types.Put(APP_SETTINGS);
  model_types.Put(EXTENSION_SETTINGS);
  model_types.Put(APP_NOTIFICATIONS);
  model_types.Put(HISTORY_DELETE_DIRECTIVES);
  model_types.Put(SYNCED_NOTIFICATIONS);
  model_types.Put(SYNCED_NOTIFICATION_APP_INFO);
  model_types.Put(DEVICE_INFO);
  model_types.Put(EXPERIMENTS);
  model_types.Put(PRIORITY_PREFERENCES);
  model_types.Put(DICTIONARY);
  model_types.Put(FAVICON_IMAGES);
  model_types.Put(FAVICON_TRACKING);

  scoped_refptr<PassiveModelWorker> passive_model_safe_worker =
      new PassiveModelWorker();
  std::vector<scoped_refptr<ModelSafeWorker>> workers;
  workers.push_back(passive_model_safe_worker);

  // Set up sync manager.
  std::unique_ptr<network::NetworkConnectionTracker>
      network_connection_tracker =
          network::TestNetworkConnectionTracker::CreateInstance();
  SyncManagerFactory sync_manager_factory(network_connection_tracker.get());
  std::unique_ptr<SyncManager> sync_manager =
      sync_manager_factory.CreateSyncManager("sync_client manager");
  LoggingJsEventHandler js_event_handler;
  // Used only by InitialProcessMetadata(), so it's okay to leave this as null.
  const scoped_refptr<base::TaskRunner> blocking_task_runner = nullptr;
  const char kUserAgent[] = "sync_client";
  // TODO(akalin): Replace this with just the context getter once
  // HttpPostProviderFactory is removed.
  auto url_loader_factory_owner =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          context_getter);
  CancelationSignal factory_cancelation_signal;
  std::unique_ptr<HttpPostProviderFactory> post_factory(new HttpBridgeFactory(
      url_loader_factory_owner->GetURLLoaderFactory()->Clone(),
      base::BindRepeating(&StubNetworkTimeUpdateCallback),
      &factory_cancelation_signal));
  post_factory->Init(kUserAgent, BindToTrackerCallback());
  // Used only when committing bookmarks, so it's okay to leave this as null.
  ExtensionsActivity* extensions_activity = nullptr;
  LoggingChangeDelegate change_delegate;
  const char kRestoredKeyForBootstrapping[] = "";
  const char kRestoredKeystoreKeyForBootstrapping[] = "";
  NullEncryptor null_encryptor;
  EngineComponentsFactoryImpl::Switches factory_switches = {
      EngineComponentsFactory::ENCRYPTION_KEYSTORE,
      EngineComponentsFactory::BACKOFF_NORMAL};
  CancelationSignal scm_cancelation_signal;

  SyncManager::InitArgs args;
  args.database_location = database_dir.GetPath();
  args.event_handler = WeakHandle<JsEventHandler>(js_event_handler.AsWeakPtr());
  args.service_url = GURL(kSyncServiceURL);
  args.post_factory = std::move(post_factory);
  args.workers = workers;
  args.extensions_activity = extensions_activity;
  args.change_delegate = &change_delegate;
  args.credentials = credentials;
  args.invalidator_client_id = invalidator_id;
  args.restored_key_for_bootstrapping = kRestoredKeyForBootstrapping;
  args.restored_keystore_key_for_bootstrapping =
      kRestoredKeystoreKeyForBootstrapping;
  args.engine_components_factory =
      std::make_unique<EngineComponentsFactoryImpl>(factory_switches);
  args.encryptor = &null_encryptor;
  args.unrecoverable_error_handler = WeakHandle<UnrecoverableErrorHandler>();
  args.report_unrecoverable_error_function =
      base::BindRepeating(LogUnrecoverableErrorContext);
  args.cancelation_signal = &scm_cancelation_signal;
  sync_manager->Init(&args);
  // TODO(akalin): Avoid passing in model parameters multiple times by
  // organizing handling of model types.
  invalidator->UpdateCredentials(credentials.email, credentials.sync_token);
  std::unique_ptr<InvalidatorShim> shim(
      new InvalidatorShim(sync_manager.get()));
  invalidator->RegisterHandler(shim.get());
  success = invalidator->UpdateRegisteredIds(
      shim.get(), ModelTypeSetToObjectIdSet(model_types));
  DCHECK(success);
  ModelTypeConnector* model_type_connector =
      sync_manager->GetModelTypeConnector();
  for (ModelType type : model_types) {
    model_type_connector->RegisterDirectoryType(type, GROUP_PASSIVE);
  }

  sync_manager->StartSyncingNormally(base::Time());

  base::RunLoop().Run();

  io_thread.Stop();
  return 0;
}

}  // namespace
}  // namespace syncer

int main(int argc, char* argv[]) {
  return syncer::SyncClientMain(argc, argv);
}
