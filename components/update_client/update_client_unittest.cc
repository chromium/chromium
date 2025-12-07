// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_parser_json.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/unpacker.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_internal.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace update_client {
namespace {

// Makes stateful mock instances for callbacks in tests. These are callback
// mocks that retain state between successive invocations of the callback.
template <typename Mock>
auto MakeMockCallback() {
  return base::BindRepeating(&Mock::Callback, base::MakeRefCounted<Mock>());
}

// Makes factories for creating update checker instances. `UpdateClient` uses
// the factory to make one update client checker for each update check. This
// factory of factories counts instances of update checkers made. The factory
// scope must enclose the scope of its clients since the `num_calls_` member
// is captured by the lambda.
template <typename MockUpdateChecker>
class MockUpdateCheckerFactory {
 public:
  typename MockUpdateChecker::Factory GetFactory() {
    return base::BindLambdaForTesting(
        [&](scoped_refptr<Configurator>) -> std::unique_ptr<UpdateChecker> {
          if constexpr (std::is_default_constructible_v<MockUpdateChecker>) {
            return std::make_unique<MockUpdateChecker>();
          } else {
            return std::make_unique<MockUpdateChecker>(++num_calls_);
          }
        });
  }

 private:
  int num_calls_ = 0;
};

// Makes a copy of the file specified by |from_path| in a temporary directory
// and returns the path of the copy. Returns true if successful. Cleans up if
// there was an error creating the copy.
bool MakeTestFile(const base::FilePath& from_path, base::FilePath* to_path) {
  base::FilePath temp_dir;
  bool result = base::CreateNewTempDirectory(FILE_PATH_LITERAL("update_client"),
                                             &temp_dir);
  if (!result) {
    return false;
  }

  base::FilePath temp_file;
  result = CreateTemporaryFileInDir(temp_dir, &temp_file);
  if (!result) {
    return false;
  }

  result = CopyFile(from_path, temp_file);
  if (!result) {
    base::DeleteFile(temp_file);
    return false;
  }

  *to_path = temp_file;
  return true;
}

class MockObserver : public UpdateClient::Observer {
 public:
  MockObserver(const MockObserver&) = delete;
  MockObserver operator=(const MockObserver&) = delete;

  explicit MockObserver(scoped_refptr<UpdateClient> update_client)
      : update_client_(update_client) {
    update_client_->AddObserver(this);
  }

  ~MockObserver() override { update_client_->RemoveObserver(this); }

  MOCK_METHOD1(OnEvent, void(const CrxUpdateItem& item));

 private:
  const scoped_refptr<UpdateClient> update_client_;
};

class MockActionHandler : public ActionHandler {
 public:
  MockActionHandler() = default;
  MockActionHandler(const MockActionHandler&) = delete;
  MockActionHandler& operator=(const MockActionHandler&) = delete;

  MOCK_METHOD3(Handle,
               void(const base::FilePath&, const std::string&, Callback));

 private:
  ~MockActionHandler() override = default;
};

class MockCrxStateChangeReceiver
    : public base::RefCountedThreadSafe<MockCrxStateChangeReceiver> {
 public:
  MOCK_METHOD(void, Receive, (const CrxUpdateItem&));

 private:
  friend class base::RefCountedThreadSafe<MockCrxStateChangeReceiver>;

  ~MockCrxStateChangeReceiver() = default;
};

class MockCrxDownloaderFactory : public CrxDownloaderFactory {
 public:
  explicit MockCrxDownloaderFactory(scoped_refptr<CrxDownloader> crx_downloader)
      : crx_downloader_(crx_downloader) {}

 private:
  ~MockCrxDownloaderFactory() override = default;

  // Overrides for CrxDownloaderFactory.
  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      const std::string& /*prod_id*/,
      bool /*background_download_enabled*/) const override {
    return crx_downloader_;
  }

  scoped_refptr<CrxDownloader> crx_downloader_;
};

// Mocks the completion callback.
auto ExpectError(Error expected_error) {
  return base::BindLambdaForTesting(
      [=](Error actual_error) { EXPECT_EQ(expected_error, actual_error); });
}

auto ExpectErrorThenQuit(base::RunLoop& runloop, Error expected_error) {
  return ExpectError(expected_error)
      .Then(base::BindLambdaForTesting([&runloop]() { runloop.Quit(); }));
}

auto ExpectErrorThenQuit(auto quit, Error expected_error) {
  return ExpectError(expected_error).Then(std::move(quit));
}

struct UpdateCheckerOptionsOneCrxUpdate {
  static constexpr int64_t kAvailableSpace = 3000;
  static constexpr size_t kComponentCount = 1;
  static constexpr std::string_view kJson = R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "jebgalgnebhfojomionfpkfelancnnkf",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx"
                    }
                  ],
                  "out": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  },
                  "size": 1015
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})";
};

struct UpdateCheckerOptionsTwoCrxUpdate {
  static constexpr int64_t kAvailableSpace = 150000;
  static constexpr size_t kComponentCount = 2;
  static constexpr std::string_view kJson = R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "jebgalgnebhfojomionfpkfelancnnkf",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx"
                    }
                  ],
                  "out": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  },
                  "size": 1015
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            }
          ]
        }
      },
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"
                    }
                  ],
                  "out": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  },
                  "size": 54014
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})";
};

struct UpdateCheckerOptionsOneCrxInstall : UpdateCheckerOptionsOneCrxUpdate {
  static constexpr std::string_view kJson = R"()]}'
{
 "response": {
   "protocol": "4.0",
   "apps": [
     {
       "appid": "jebgalgnebhfojomionfpkfelancnnkf",
       "status": "ok",
       "updatecheck": {
         "status": "ok",
         "nextversion": "1.0",
         "pipelines": [
           {
             "pipeline_id": "pipe1",
             "operations": [
               {
                 "type": "download",
                 "urls": [
                   {
                     "url": "http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx"
                   }
                 ],
                 "out": {
                   "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                 },
                 "size": 1015
               },
               {
                 "type": "crx3",
                 "arguments": "--arg1 --arg2",
                 "path": "UpdaterSetup.exe",
                 "in": {
                   "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                 }
               }
             ]
           }
         ]
       }
     }
   ]
 }
})";
};

struct UpdateCheckerOptionsTwoCrxUpdateServerIgnoresSecond
    : UpdateCheckerOptionsOneCrxInstall {
  static constexpr size_t kComponentCount = 2;
};

struct UpdateCheckerOptionsTwoCrxUpdateNoUpdate {
  static constexpr int64_t kAvailableSpace = 3000;
  static constexpr size_t kComponentCount = 2;
  static constexpr std::string_view kJson = R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "jebgalgnebhfojomionfpkfelancnnkf",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx"
                    }
                  ],
                  "out": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  },
                  "size": 1015
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            }
          ]
        }
      },
      {
        "appid": "abagagagagagagagagagagagagagagag",
        "status": "ok",
        "updatecheck": {
          "status": "noupdate"
        }
      }
    ]
  }
})";
};

struct UpdateCheckerOptionsActionRunNoUpdate {
  static constexpr int64_t kAvailableSpace = 0;
  static constexpr size_t kComponentCount = 1;
  static constexpr std::string_view kJson = R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "gjpmebpgbhcamgdgjcmnjfhggjpgcimm",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "run",
                  "path": "ChromeRecovery.crx3"
                }
              ]
            }
          ]
        }
      }
    ]
  }
})";
};

struct UpdateCheckerOptionsOneCrxInstallDiskFull
    : UpdateCheckerOptionsOneCrxInstall {
  static constexpr int64_t kAvailableSpace = 0;
};

struct UpdateCheckerOptionsUnsupportedOperationType
    : UpdateCheckerOptionsOneCrxUpdate {
  static constexpr std::string_view kJson = R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "jebgalgnebhfojomionfpkfelancnnkf",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "badoperation"
                },
                {
                  "type": "crx3",
                  "arguments": "--arg1 --arg2",
                  "path": "UpdaterSetup.exe",
                  "in": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})";
};

template <typename T>
concept IsUpdateCheckerOptions = requires(T t) {
  T::kAvailableSpace;
  T::kComponentCount;
  T::kJson;
};

template <typename Options>
  requires IsUpdateCheckerOptions<Options>
class MockUpdateCheckerImpl : public UpdateChecker {
 public:
  void CheckForUpdates(
      scoped_refptr<UpdateContext> context,
      const base::flat_map<std::string, std::string>& additional_attributes,
      UpdateCheckCallback update_check_callback) override {
    context->get_available_space = base::BindRepeating(
        [](const base::FilePath&) { return Options::kAvailableSpace; });
    base::expected<ProtocolParser::Results, std::string> results =
        ProtocolParserJSON::ParseJSON(std::string(Options::kJson));
    EXPECT_TRUE(results.has_value()) << results.error();
    EXPECT_FALSE(context->session_id.empty());
    EXPECT_EQ(context->components_to_check_for_updates.size(),
              Options::kComponentCount);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(update_check_callback),
                                  results.value(), ErrorCategory::kNone, 0, 0));
  }
};

class MockUpdateCheckerAlwaysFails : public UpdateChecker {
 public:
  MockUpdateCheckerAlwaysFails() = default;

  void CheckForUpdates(
      scoped_refptr<UpdateContext> context,
      const base::flat_map<std::string, std::string>& additional_attributes,
      UpdateCheckCallback update_check_callback) override {
    ADD_FAILURE() << "Check for update failed successfully.";
  }
};

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Truly;
using ::testing::Unused;

}  // namespace

class MockPingManagerImpl : public PingManager {
 public:
  struct PingData {
    std::string id;
    base::Version previous_version;
    base::Version next_version;
    ErrorCategory error_category = ErrorCategory::kNone;
    int error_code = 0;
    int extra_code1 = 0;
    int event_type;
    int event_result;
    std::string pipeline_id;
  };

  explicit MockPingManagerImpl(scoped_refptr<Configurator> config);
  MockPingManagerImpl(const MockPingManagerImpl&) = delete;
  MockPingManagerImpl& operator=(const MockPingManagerImpl&) = delete;

  void SendPing(const std::string& session_id,
                const CrxComponent& component,
                std::vector<base::Value::Dict> events,
                base::OnceClosure callback) override;

  const std::vector<PingData>& ping_data() const;
  const std::vector<PingData>& terminal_ping_data() const;

  const std::vector<base::Value::Dict>& events() const;

 protected:
  ~MockPingManagerImpl() override;

 private:
  std::vector<PingData> ping_data_;
  std::vector<PingData> terminal_ping_data_;
  std::vector<base::Value::Dict> events_;
};

MockPingManagerImpl::MockPingManagerImpl(scoped_refptr<Configurator> config)
    : PingManager(config) {}

MockPingManagerImpl::~MockPingManagerImpl() = default;

void MockPingManagerImpl::SendPing(const std::string& session_id,
                                   const CrxComponent& component,
                                   std::vector<base::Value::Dict> events,
                                   base::OnceClosure callback) {
  for (const base::Value::Dict& event : events) {
    PingData ping_data;
    ping_data.id = component.app_id;
    int event_type = event.FindInt("eventtype").value_or(0);
    const std::string* previous_version = event.FindString("previousversion");
    if (previous_version) {
      ping_data.previous_version = base::Version(*previous_version);
    }
    ping_data.event_result = event.FindInt("eventresult").value_or(0);
    const std::string* next_version = event.FindString("nextversion");
    if (next_version) {
      ping_data.next_version = base::Version(*next_version);
    }
    std::optional<int> error_category = event.FindInt("errorcat");
    if (error_category) {
      ping_data.error_category = static_cast<ErrorCategory>(*error_category);
    }
    std::optional<int> error_code = event.FindInt("errorcode");
    if (error_code) {
      ping_data.error_code = *error_code;
    }
    std::optional<int> extra_code1 = event.FindInt("extracode1");
    if (extra_code1) {
      ping_data.extra_code1 = *extra_code1;
    }
    const std::string* pipeline_id = event.FindString("pipeline_id");
    if (pipeline_id) {
      ping_data.pipeline_id = *pipeline_id;
    }
    if (event_type != 0) {
      ping_data.event_type = event_type;
    }
    ping_data_.push_back(ping_data);
    if (event_type == protocol_request::kEventInstall ||
        event_type == protocol_request::kEventUpdate ||
        event_type == protocol_request::kEventUninstall) {
      terminal_ping_data_.push_back(ping_data);
    }
  }
  events_ = std::move(events);

  std::move(callback).Run();
}

const std::vector<MockPingManagerImpl::PingData>&
MockPingManagerImpl::ping_data() const {
  return ping_data_;
}

const std::vector<MockPingManagerImpl::PingData>&
MockPingManagerImpl::terminal_ping_data() const {
  return terminal_ping_data_;
}

const std::vector<base::Value::Dict>& MockPingManagerImpl::events() const {
  return events_;
}

class UpdateClientTest : public testing::Test {
 private:
  // Must be initialized before `runloop_`.
  base::test::TaskEnvironment task_environment_;

 protected:
  UpdateClientTest() {
    RegisterPersistedDataPrefs(pref_->registry());
    config_ = base::MakeRefCounted<TestConfigurator>(pref_.get());
  }

  scoped_refptr<update_client::TestConfigurator> config() { return config_; }

  // Injects the CrxDownloaderFactory in the test fixture.
  template <typename MockCrxDownloaderT>
  void SetMockCrxDownloader() {
    config()->SetCrxDownloaderFactory(
        base::MakeRefCounted<MockCrxDownloaderFactory>(
            base::MakeRefCounted<MockCrxDownloaderT>()));
  }

  base::RunLoop runloop_;

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_ =
      std::make_unique<TestingPrefServiceSimple>();
  scoped_refptr<update_client::TestConfigurator> config_;
};

// Tests the scenario where one update check is done for one CRX. The CRX
// has no update.
TEST_F(UpdateClientTest, OneCrxNoUpdate) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::vector<std::optional<CrxComponent>> component = {crx};
      std::move(callback).Run({component});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      auto& component = context->components.at(id);

      EXPECT_TRUE(component->is_foreground());

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";

      ProtocolParser::Results results;
      results.apps.push_back(result);

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kUpToDate;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  update_client->Update(
      {"jebgalgnebhfojomionfpkfelancnnkf"},
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver), true,
      ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(2u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kUpToDate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
}

// Tests the scenario where two CRXs are checked for updates. On CRX has
// an update, the other CRX does not.
TEST_F(UpdateClientTest, TwoCrxUpdateNoUpdate) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx1;
      crx1.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx1.name = "test_jebg";
      crx1.pk_hash = base::ToVector(jebg_hash);
      crx1.version = base::Version("0.9");
      crx1.installer = base::MakeRefCounted<TestInstaller>();
      crx1.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      CrxComponent crx2;
      crx2.app_id = "abagagagagagagagagagagagagagagag";
      crx2.name = "test_abag";
      crx2.pk_hash = base::ToVector(abag_hash);
      crx2.version = base::Version("2.2");
      crx2.installer = base::MakeRefCounted<TestInstaller>();
      crx2.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx1, crx2});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsTwoCrxUpdateNoUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      download_metrics.url = url;
      download_metrics.downloader = DownloadMetrics::kNone;
      download_metrics.error = 0;
      download_metrics.downloaded_bytes = 1015;
      download_metrics.total_bytes = 1015;
      download_metrics.download_time_ms = 1000;

      base::FilePath path;
      EXPECT_TRUE(MakeTestFile(
          GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

      Result result;
      result.error = 0;
      result.response = path;

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes / 2,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(1u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(0, ping_data[0].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kChecking &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kCanUpdate &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kDownloading &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kUpdating &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kUpdated &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })));
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kChecking &&
                         item.id == "abagagagagagagagagagagagagagagag";
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kUpToDate &&
                         item.id == "abagagagagagagagagagagagagagagag";
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf",
                                        "abagagagagagagagagagagagagagagag"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(9u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kChecking, items[1].state);
  EXPECT_EQ("abagagagagagagagagagagagagagagag", items[1].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kDownloading, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
  EXPECT_EQ(ComponentState::kDownloading, items[5].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);
  EXPECT_EQ(ComponentState::kUpdating, items[6].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[6].id);
  EXPECT_EQ(ComponentState::kUpdated, items[7].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[7].id);
  EXPECT_EQ(ComponentState::kUpToDate, items[8].state);
  EXPECT_EQ("abagagagagagagagagagagagagagagag", items[8].id);

  std::vector<std::tuple<int64_t, int64_t>> progress_bytes = {
      {-1, -1},     {-1, -1},     {-1, -1},     {-1, -1}, {507, 1015},
      {1015, 1015}, {1015, 1015}, {1015, 1015}, {-1, -1}};
  EXPECT_EQ(items.size(), progress_bytes.size());
  for (size_t i{0}; i != items.size(); ++i) {
    EXPECT_EQ(items[i].downloaded_bytes, std::get<0>(progress_bytes[i]));
    EXPECT_EQ(items[i].total_bytes, std::get<1>(progress_bytes[i]));
  }
}

// Tests the scenario where two CRXs are checked for updates. One CRX has
// an update but the server ignores the second CRX and returns no response for
// it. The second component gets an |UPDATE_RESPONSE_NOT_FOUND| error and
// transitions to the error state.
TEST_F(UpdateClientTest, TwoCrxUpdateFirstServerIgnoresSecond) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx1;
      crx1.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx1.name = "test_jebg";
      crx1.pk_hash = base::ToVector(jebg_hash);
      crx1.version = base::Version("0.9");
      crx1.installer = base::MakeRefCounted<TestInstaller>();
      crx1.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      CrxComponent crx2;
      crx2.name = "test_abag";
      crx2.pk_hash = base::ToVector(abag_hash);
      crx2.version = base::Version("2.2");
      crx2.installer = base::MakeRefCounted<TestInstaller>();
      crx2.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx1, crx2});
    }
  };

  // Note: even though 2 appid's are requested, we are intentionally only
  // sending the first.
  MockUpdateCheckerFactory<MockUpdateCheckerImpl<
      UpdateCheckerOptionsTwoCrxUpdateServerIgnoresSecond>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      download_metrics.url = url;
      download_metrics.downloader = DownloadMetrics::kNone;
      download_metrics.error = 0;
      download_metrics.downloaded_bytes = 1015;
      download_metrics.total_bytes = 1015;
      download_metrics.download_time_ms = 1000;

      base::FilePath path;
      EXPECT_TRUE(MakeTestFile(
          GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

      Result result;
      result.error = 0;
      result.response = path;

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(1u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(0, ping_data[0].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kChecking &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kCanUpdate &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kDownloading &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kUpdating &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kUpdated &&
                         item.id == "jebgalgnebhfojomionfpkfelancnnkf";
                })));
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kChecking &&
                         item.id == "abagagagagagagagagagagagagagagag";
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.state == ComponentState::kUpdateError &&
                         item.id == "abagagagagagagagagagagagagagagag";
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(5, static_cast<int>(item.error_category));
          EXPECT_EQ(-10004, item.error_code);
          EXPECT_EQ(0, item.extra_code1);
        });
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf",
                                        "abagagagagagagagagagagagagagagag"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(8u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kChecking, items[1].state);
  EXPECT_EQ("abagagagagagagagagagagagagagagag", items[1].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kDownloading, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
  EXPECT_EQ(ComponentState::kUpdating, items[5].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);
  EXPECT_EQ(ComponentState::kUpdated, items[6].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[6].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[7].state);
  EXPECT_EQ("abagagagagagagagagagagagagagagag", items[7].id);
}

// Tests the update check for two CRXs scenario when the second CRX does not
// provide a CrxComponent instance. In this case, the update is handled as
// if only one component were provided as an argument to the |Update| call
// with the exception that the second component still fires an event such as
// |COMPONENT_UPDATE_ERROR|.
TEST_F(UpdateClientTest, TwoCrxUpdateNoCrxComponentData) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx, std::nullopt});
    }
  };

  // Note: even though 2 appid's are requested, since "ihfo..." has no component
  // data, the response only contains the first appid's update response.
  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/jebgalgnebhfojomionfpkfelancnnkf.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 1015;
        download_metrics.total_bytes = 1015;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(1u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(0, ping_data[0].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdated;
                })));
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf",
                                        "ihfokbkgjpifnbbojhneepfflplebdkc"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(7u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[1].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kDownloading, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
  EXPECT_EQ(ComponentState::kUpdating, items[5].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);
  EXPECT_EQ(ComponentState::kUpdated, items[6].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[6].id);
}

// Tests the update check for two CRXs scenario when no CrxComponent data is
// provided for either component. In this case, no update check occurs, and
// |COMPONENT_UPDATE_ERROR| event fires for both components.
TEST_F(UpdateClientTest, TwoCrxUpdateNoCrxComponentDataAtAll) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      std::move(callback).Run({std::nullopt, std::nullopt});
    }
  };

  MockUpdateCheckerFactory<MockUpdateCheckerAlwaysFails>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      EXPECT_EQ(0u, MockPingManagerImpl::ping_data().size());
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf",
                                        "ihfokbkgjpifnbbojhneepfflplebdkc"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(2u, items.size());
  EXPECT_EQ(ComponentState::kUpdateError, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[1].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
}

// Tests the scenario where there is a download timeout for the first
// CRX. The update for the first CRX fails. The update client waits before
// attempting the update for the second CRX. This update succeeds.
TEST_F(UpdateClientTest, TwoCrxUpdateDownloadTimeout) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx1;
      crx1.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx1.name = "test_jebg";
      crx1.pk_hash = base::ToVector(jebg_hash);
      crx1.version = base::Version("0.9");
      crx1.installer = base::MakeRefCounted<TestInstaller>();
      crx1.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      CrxComponent crx2;
      crx2.app_id = "ihfokbkgjpifnbbojhneepfflplebdkc";
      crx2.name = "test_ihfo";
      crx2.pk_hash = base::ToVector(ihfo_hash);
      crx2.version = base::Version("0.8");
      crx2.installer = base::MakeRefCounted<TestInstaller>();
      crx2.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx1, crx2});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsTwoCrxUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/jebgalgnebhfojomionfpkfelancnnkf.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = -118;
        download_metrics.downloaded_bytes = 0;
        download_metrics.total_bytes = 0;
        download_metrics.download_time_ms = 1000;

        // The result must not include a file path in the case of errors.
        result.error = -118;
      } else if (url.GetPath() ==
                 "/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 54014;
        download_metrics.total_bytes = 54014;
        download_metrics.download_time_ms = 2000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(2u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(1, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(-118, ping_data[0].error_code);
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[1].id);
      EXPECT_EQ(base::Version("0.8"), ping_data[1].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[1].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[1].error_category));
      EXPECT_EQ(0, ping_data[1].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(1, static_cast<int>(item.error_category));
          EXPECT_EQ(-118, item.error_code);
          EXPECT_EQ(0, item.extra_code1);
        });
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdated;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf",
                                        "ihfokbkgjpifnbbojhneepfflplebdkc"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(11u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kChecking, items[1].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kDownloading, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[5].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[6].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[6].id);
  EXPECT_EQ(ComponentState::kDownloading, items[7].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[7].id);
  EXPECT_EQ(ComponentState::kDownloading, items[8].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[8].id);
  EXPECT_EQ(ComponentState::kUpdating, items[9].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[9].id);
  EXPECT_EQ(ComponentState::kUpdated, items[10].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[10].id);
}

// Tests the differential update scenario for one CRX. Tests install progress
// for differential and full updates.
TEST_F(UpdateClientTest, OneCrxDiffUpdate) {
  class DataCallbackMock : public base::RefCountedThreadSafe<DataCallbackMock> {
   public:
    DataCallbackMock() {
      installer_->set_installer_progress_samples({-1, 50, 100});
    }

    void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      ++num_calls_;

      CrxComponent crx;
      crx.app_id = "ihfokbkgjpifnbbojhneepfflplebdkc";
      crx.name = "test_ihfo";
      crx.pk_hash = base::ToVector(ihfo_hash);
      crx.installer = installer_;
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      if (num_calls_ == 1) {
        crx.version = base::Version("0.8");
      } else if (num_calls_ == 2) {
        crx.version = base::Version("1.0");
      } else {
        ADD_FAILURE();
      }

      std::move(callback).Run({crx});
    }

   private:
    friend class base::RefCountedThreadSafe<DataCallbackMock>;
    ~DataCallbackMock() = default;

    int num_calls_ = 0;
    scoped_refptr<VersionedTestInstaller> installer_ =
        base::MakeRefCounted<VersionedTestInstaller>();
  };
  auto data_callback_mock = MakeMockCallback<DataCallbackMock>();

  class MockUpdateChecker : public UpdateChecker {
   public:
    explicit MockUpdateChecker(int num_calls) : num_calls_(num_calls) {}

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      context->get_available_space = base::BindRepeating(
          [](const base::FilePath&) -> int64_t { return 200000; });
      base::expected<ProtocolParser::Results, std::string> results;
      if (num_calls_ == 1) {
        context->get_available_space = base::BindRepeating(
            [](const base::FilePath&) -> int64_t { return 150000; });
        results = ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"
                    }
                  ],
                  "out": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  },
                  "size": 54014
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      } else if (num_calls_ == 2) {
        context->get_available_space = base::BindRepeating(
            [](const base::FilePath&) -> int64_t { return 4000; });
        results = ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "2.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.puff"
                    }
                  ],
                  "out": {
                    "sha256": "f2254da51fa2478a8ba90e58e1c28e24033ec7841015eebf1c82e31b957c44b2"
                  },
                  "size": 1680
                },
                {
                  "type": "puff",
                  "previous": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  },
                  "out": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      } else {
        ADD_FAILURE();
      }
      EXPECT_TRUE(results.has_value()) << results.error();
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(context->components_to_check_for_updates.size(), 1u);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(update_check_callback), results.value(),
                         ErrorCategory::kNone, 0, 0));
    }

   private:
    const int num_calls_;
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 54014;
        download_metrics.total_bytes = 54014;
        download_metrics.download_time_ms = 2000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"), &path));

        result.error = 0;
        result.response = path;
      } else if (url.GetPath() ==
                 "/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.puff") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 1680;
        download_metrics.total_bytes = 1680;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("ihfokbkgjpifnbbojhneepfflplebdkc_1to2.puff"),
            &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE() << url.GetPath();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes / 2,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(2u, ping_data.size());
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[0].id);
      EXPECT_EQ(base::Version("0.8"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(0, ping_data[0].error_code);
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[1].id);
      EXPECT_EQ(base::Version("1.0"), ping_data[1].previous_version);
      EXPECT_EQ(base::Version("2.0"), ping_data[1].next_version);
      EXPECT_EQ(0, ping_data[1].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })))
        .Times(3);
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdated;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })))
        .Times(3);
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdated;
                })));
  }

  const std::vector<std::string> ids = {"ihfokbkgjpifnbbojhneepfflplebdkc"};
  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();
    EXPECT_EQ(10u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
    EXPECT_EQ(ComponentState::kDownloading, items[2].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[2].id);
    EXPECT_EQ(ComponentState::kDownloading, items[3].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[3].id);
    EXPECT_EQ(ComponentState::kDownloading, items[4].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[4].id);
    EXPECT_EQ(ComponentState::kUpdating, items[5].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[5].id);
    EXPECT_EQ(ComponentState::kUpdating, items[6].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[6].id);
    EXPECT_EQ(ComponentState::kUpdating, items[7].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[7].id);
    EXPECT_EQ(ComponentState::kUpdating, items[8].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[8].id);
    EXPECT_EQ(ComponentState::kUpdated, items[9].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[9].id);

    std::vector samples = {-1, -1, -1, -1, -1, -1, -1, 50, 100, 100};
    EXPECT_EQ(items.size(), samples.size());
    for (size_t i = 0; i != items.size(); ++i) {
      EXPECT_EQ(items[i].install_progress, samples[i]);
    }
  }

  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();

    EXPECT_EQ(10u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
    EXPECT_EQ(ComponentState::kDownloading, items[2].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[2].id);
    EXPECT_EQ(ComponentState::kDownloading, items[3].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[3].id);
    EXPECT_EQ(ComponentState::kDownloading, items[4].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[4].id);
    EXPECT_EQ(ComponentState::kUpdating, items[5].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[5].id);
    EXPECT_EQ(ComponentState::kUpdating, items[6].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[6].id);
    EXPECT_EQ(ComponentState::kUpdating, items[7].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[7].id);
    EXPECT_EQ(ComponentState::kUpdating, items[8].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[8].id);
    EXPECT_EQ(ComponentState::kUpdated, items[9].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[9].id);

    std::vector<int> samples = {-1, -1, -1, -1, -1, -1, -1, 50, 100, 100};
    EXPECT_EQ(items.size(), samples.size());
    for (size_t i = 0; i != items.size(); ++i) {
      EXPECT_EQ(items[i].install_progress, samples[i]);
    }
  }
}

// Tests the update scenario for one CRX where the CRX installer returns
// an error. Tests that the |unpack_path| argument refers to a valid path
// then |Install| is called, then tests that the |unpack| path is deleted
// by the |update_client| code before the test ends.
TEST_F(UpdateClientTest, OneCrxInstallError) {
  class MockInstaller : public CrxInstaller {
   public:
    MOCK_METHOD1(DoInstall, void(const base::FilePath& unpack_path));
    MOCK_METHOD1(GetInstalledFile,
                 std::optional<base::FilePath>(const std::string& file));
    MOCK_METHOD0(Uninstall, bool());

    void Install(const base::FilePath& unpack_path,
                 const std::string& public_key,
                 std::unique_ptr<InstallParams> /*install_params*/,
                 ProgressCallback progress_callback,
                 Callback callback) override {
      DoInstall(unpack_path);

      unpack_path_ = unpack_path;
      EXPECT_TRUE(base::DirectoryExists(unpack_path_));
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(
              std::move(callback),
              CrxInstaller::Result(
                  {.category = ErrorCategory::kInstaller,
                   .code = static_cast<int>(InstallError::GENERIC_ERROR)})));
    }

   protected:
    ~MockInstaller() override {
      // The unpack path is deleted unconditionally by the component state code,
      // which is driving this installer. Therefore, the unpack path must not
      // exist when this object is destroyed.
      if (!unpack_path_.empty()) {
        EXPECT_FALSE(base::DirectoryExists(unpack_path_));
      }
    }

   private:
    // Contains the |unpack_path| argument of the Install call.
    base::FilePath unpack_path_;
  };

  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      scoped_refptr<MockInstaller> installer =
          base::MakeRefCounted<MockInstaller>();

      EXPECT_CALL(*installer, DoInstall(_));
      EXPECT_CALL(*installer, GetInstalledFile(_)).Times(0);
      EXPECT_CALL(*installer, Uninstall()).Times(0);

      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = installer;
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxInstall>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      download_metrics.url = url;
      download_metrics.downloader = DownloadMetrics::kNone;
      download_metrics.error = 0;
      download_metrics.downloaded_bytes = 1015;
      download_metrics.total_bytes = 1015;
      download_metrics.download_time_ms = 1000;

      base::FilePath path;
      EXPECT_TRUE(MakeTestFile(
          GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

      Result result;
      result.error = 0;
      result.response = path;

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::ping_data();
      EXPECT_EQ(3u, ping_data.size());
      // Expect that the download ping carries the pipeline id.
      EXPECT_EQ(ping_data[0].pipeline_id, "pipe1");  // Download
      EXPECT_EQ(ping_data[0].error_category, ErrorCategory::kNone);
      EXPECT_EQ(static_cast<CrxDownloaderError>(ping_data[0].error_code),
                CrxDownloaderError::NONE);

      EXPECT_EQ(ping_data[1].pipeline_id, "pipe1");  // crx3
      EXPECT_EQ(ping_data[1].error_category, ErrorCategory::kInstaller);
      EXPECT_EQ(static_cast<InstallError>(ping_data[1].error_code),
                InstallError::GENERIC_ERROR);

      EXPECT_EQ(ping_data[2].id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(ping_data[2].previous_version, base::Version("0.9"));
      EXPECT_EQ(ping_data[2].next_version, base::Version("1.0"));
      EXPECT_EQ(ping_data[2].error_category, ErrorCategory::kInstaller);
      EXPECT_EQ(static_cast<InstallError>(ping_data[2].error_code),
                InstallError::GENERIC_ERROR);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  update_client->Update(
      {"jebgalgnebhfojomionfpkfelancnnkf"},
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(6u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kDownloading, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kUpdating, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[5].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);
}

// Tests the fallback from differential to full update scenario for one CRX.
TEST_F(UpdateClientTest, OneCrxDiffUpdateFailsFullUpdateSucceeds) {
  class DataCallbackMock : public base::RefCountedThreadSafe<DataCallbackMock> {
   public:
    void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      ++num_calls_;

      CrxComponent crx;
      crx.app_id = "ihfokbkgjpifnbbojhneepfflplebdkc";
      crx.name = "test_ihfo";
      crx.pk_hash = base::ToVector(ihfo_hash);
      crx.installer = installer_;
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      if (num_calls_ == 1) {
        crx.version = base::Version("0.8");
      } else if (num_calls_ == 2) {
        crx.version = base::Version("1.0");
      } else {
        ADD_FAILURE();
      }

      std::move(callback).Run({crx});
    }

   private:
    friend class base::RefCountedThreadSafe<DataCallbackMock>;
    ~DataCallbackMock() = default;

    int num_calls_ = 0;
    scoped_refptr<VersionedTestInstaller> installer_ =
        base::MakeRefCounted<VersionedTestInstaller>();
  };
  auto data_callback_mock = MakeMockCallback<DataCallbackMock>();

  class MockUpdateChecker : public UpdateChecker {
   public:
    explicit MockUpdateChecker(int num_calls) : num_calls_(num_calls) {}

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      base::expected<ProtocolParser::Results, std::string> results;
      context->get_available_space = base::BindRepeating(
          [](const base::FilePath&) -> int64_t { return 150000; });

      if (num_calls_ == 1) {
        results = ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"
                    }
                  ],
                  "out": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  },
                  "size": 54014
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      } else if (num_calls_ == 2) {
        results = ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "2.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.puff"
                    }
                  ],
                  "out": {
                    "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
                  },
                  "size": 1680
                },
                {
                  "type": "puff",
                  "previous": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  },
                  "out": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                }
              ]
            },
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"
                    }
                  ],
                  "out": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  },
                  "size": 54409
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      } else {
        ADD_FAILURE();
      }
      EXPECT_TRUE(results.has_value()) << results.error();
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(context->components_to_check_for_updates.size(), 1u);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(update_check_callback), results.value(),
                         ErrorCategory::kNone, 0, 0));
    }

   private:
    const int num_calls_;
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 54014;
        download_metrics.total_bytes = 54014;
        download_metrics.download_time_ms = 2000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"), &path));

        result.error = 0;
        result.response = path;
      } else if (url.GetPath() ==
                 "/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.puff") {
        // A download error is injected on this execution path.
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = -1;
        download_metrics.downloaded_bytes = 0;
        download_metrics.total_bytes = 1680;
        download_metrics.download_time_ms = 1000;

        // The response must not include a file path in the case of errors.
        result.error = -1;
      } else if (url.GetPath() ==
                 "/download/ihfokbkgjpifnbbojhneepfflplebdkc_2.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 54409;
        download_metrics.total_bytes = 54409;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"), &path));

        result.error = 0;
        result.response = path;
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(2u, ping_data.size());
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[0].id);
      EXPECT_EQ(base::Version("0.8"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(0, ping_data[0].error_code);
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[1].id);
      EXPECT_EQ(base::Version("1.0"), ping_data[1].previous_version);
      EXPECT_EQ(base::Version("2.0"), ping_data[1].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[1].error_category));
      EXPECT_EQ(0, ping_data[1].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdated;
                })));

    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdated;
                })));
  }

  const std::vector<std::string> ids = {"ihfokbkgjpifnbbojhneepfflplebdkc"};

  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();
    EXPECT_EQ(6u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
    EXPECT_EQ(ComponentState::kDownloading, items[2].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[2].id);
    EXPECT_EQ(ComponentState::kDownloading, items[3].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[3].id);
    EXPECT_EQ(ComponentState::kUpdating, items[4].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[4].id);
    EXPECT_EQ(ComponentState::kUpdated, items[5].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[5].id);
  }

  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();

    EXPECT_EQ(8u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
    EXPECT_EQ(ComponentState::kDownloading, items[2].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[2].id);
    EXPECT_EQ(ComponentState::kDownloading, items[3].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[3].id);
    EXPECT_EQ(ComponentState::kDownloading, items[4].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[4].id);
    EXPECT_EQ(ComponentState::kDownloading, items[5].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[5].id);
    EXPECT_EQ(ComponentState::kUpdating, items[6].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[6].id);
    EXPECT_EQ(ComponentState::kUpdated, items[7].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[7].id);
  }
}

// Tests the queuing of update checks. In this scenario, two update checks are
// done for one CRX. The second update check call is queued up and will run
// after the first check has completed. The CRX has no updates.
TEST_F(UpdateClientTest, OneCrxNoUpdateQueuedCall) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      auto& component = context->components.at(id);

      EXPECT_FALSE(component->is_foreground());

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";
      ProtocolParser::Results results;
      results.apps.push_back(result);

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
  }

  std::vector<CrxUpdateItem> items1;
  auto receiver1 = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver1, Receive(_))
      .WillRepeatedly(
          [&items1](const CrxUpdateItem& item) { items1.push_back(item); });

  std::vector<CrxUpdateItem> items2;
  auto receiver2 = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver2, Receive(_))
      .WillRepeatedly(
          [&items2](const CrxUpdateItem& item) { items2.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver1),
      false, ExpectError(Error::NONE));
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver2),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(2u, items1.size());
  EXPECT_EQ(ComponentState::kChecking, items1[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items1[0].id);
  EXPECT_EQ(ComponentState::kUpToDate, items1[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items1[1].id);

  EXPECT_EQ(2u, items2.size());
  EXPECT_EQ(ComponentState::kChecking, items2[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items2[0].id);
  EXPECT_EQ(ComponentState::kUpToDate, items2[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items2[1].id);
}

// Tests the install of one CRX. Tests the installer is invoked with the
// run and arguments values of the manifest object. Tests that "pv" and "fp"
// are persisted.
TEST_F(UpdateClientTest, OneCrxInstall) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.0");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxInstall>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/jebgalgnebhfojomionfpkfelancnnkf.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 1015;
        download_metrics.total_bytes = 1015;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(1u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.0"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(0, ping_data[0].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());
  {
    EXPECT_FALSE(config()->GetPrefService()->FindPreference(
        "updateclientdata.apps.jebgalgnebhfojomionfpkfelancnnkf.pv"));
    EXPECT_FALSE(config()->GetPrefService()->FindPreference(
        "updateclientdata.apps.jebgalgnebhfojomionfpkfelancnnkf.fp"));
  }

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdated;
                })))

        .WillOnce([](const CrxUpdateItem& item) {
          ASSERT_TRUE(item.component);
          const auto* test_installer =
              static_cast<TestInstaller*>(item.component->installer.get());
          EXPECT_EQ("UpdaterSetup.exe", test_installer->install_params()->run);
          EXPECT_EQ("--arg1 --arg2",
                    test_installer->install_params()->arguments);
        });
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  update_client->Install(
      std::string("jebgalgnebhfojomionfpkfelancnnkf"),
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(6u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kDownloading, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kUpdating, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
  EXPECT_EQ(ComponentState::kUpdated, items[5].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);

  const base::Value::Dict& dict =
      config()->GetPrefService()->GetDict("updateclientdata");
  EXPECT_EQ("1.0", CHECK_DEREF(dict.FindStringByDottedPath(
                       "apps.jebgalgnebhfojomionfpkfelancnnkf.pv")));
}

// Tests the install of one CRX when no component data is provided. This
// results in an install error.
TEST_F(UpdateClientTest, OneCrxInstallNoCrxComponentData) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      std::move(callback).Run({std::nullopt});
    }
  };

  MockUpdateCheckerFactory<MockUpdateCheckerAlwaysFails>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      EXPECT_EQ(0u, MockPingManagerImpl::ping_data().size());
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })))

        .WillOnce([](const CrxUpdateItem& item) {
          // Tests that the state of the component when the CrxComponent data
          // is not provided. In this case, the optional |item.component|
          // instance is not present.
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", item.id);
          EXPECT_FALSE(item.component);
          EXPECT_EQ(ErrorCategory::kService, item.error_category);
          EXPECT_EQ(static_cast<int>(Error::CRX_NOT_FOUND), item.error_code);
          EXPECT_EQ(0, item.extra_code1);
        });
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  update_client->Install(
      std::string("jebgalgnebhfojomionfpkfelancnnkf"),
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(1u, items.size());
  EXPECT_EQ(ComponentState::kUpdateError, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
}

// Tests that overlapping installs of the same CRX result in an error.
TEST_F(UpdateClientTest, ConcurrentInstallSameCRX) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.0");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";

      ProtocolParser::Results results;
      results.apps.push_back(result);

      // Verify that calling Install sets |is_foreground| for the component.
      EXPECT_TRUE(context->components.at(id)->is_foreground());

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                       item.state == ComponentState::kChecking;
              })));
  EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                       item.state == ComponentState::kUpToDate;
              })));

  std::vector<CrxUpdateItem> items1;
  auto receiver1 = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver1, Receive(_))
      .WillRepeatedly(
          [&items1](const CrxUpdateItem& item) { items1.push_back(item); });

  std::vector<CrxUpdateItem> items2;
  auto receiver2 = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver2, Receive(_))
      .WillRepeatedly(
          [&items2](const CrxUpdateItem& item) { items2.push_back(item); });

  base::RepeatingClosure barrier_quit_closure =
      BarrierClosure(2, runloop_.QuitClosure());

  update_client->Install(
      std::string("jebgalgnebhfojomionfpkfelancnnkf"),
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver1),
      ExpectErrorThenQuit(barrier_quit_closure, Error::NONE));
  update_client->Install(
      std::string("jebgalgnebhfojomionfpkfelancnnkf"),
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver2),
      ExpectErrorThenQuit(barrier_quit_closure, Error::UPDATE_IN_PROGRESS));
  runloop_.Run();

  EXPECT_EQ(2u, items1.size());
  EXPECT_EQ(ComponentState::kChecking, items1[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items1[0].id);
  EXPECT_EQ(ComponentState::kUpToDate, items1[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items1[1].id);

  EXPECT_TRUE(items2.empty());
}

// Tests that UpdateClient::Update returns Error::INVALID_ARGUMENT when
// the |ids| parameter is empty.
TEST_F(UpdateClientTest, EmptyIdList) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      std::move(callback).Run({});
    }
  };


  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };
  MockUpdateCheckerFactory<MockUpdateCheckerAlwaysFails>
      mock_update_checker_factory;

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  const std::vector<std::string> empty_id_list;
  update_client->Update(empty_id_list,
                        base::BindOnce(&DataCallbackMock::Callback), {}, false,
                        ExpectErrorThenQuit(runloop_, Error::INVALID_ARGUMENT));
  runloop_.Run();
}

TEST_F(UpdateClientTest, DiskFull) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx1;
      crx1.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx1.name = "test_jebg";
      crx1.pk_hash = base::ToVector(jebg_hash);
      crx1.version = base::Version("0.9");
      crx1.installer = base::MakeRefCounted<TestInstaller>();
      crx1.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx1});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxInstallDiskFull>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(1u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(1, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(static_cast<int>(CrxDownloaderError::DISK_FULL),
                ping_data[0].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(4u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kDownloading, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
}

TEST_F(UpdateClientTest, DiskFullDiff) {
  class DataCallbackMock : public base::RefCountedThreadSafe<DataCallbackMock> {
   public:
    DataCallbackMock() {
      installer_->set_installer_progress_samples({-1, 50, 100});
    }

    void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      ++num_calls_;

      CrxComponent crx;
      crx.app_id = "ihfokbkgjpifnbbojhneepfflplebdkc";
      crx.name = "test_ihfo";
      crx.pk_hash = base::ToVector(ihfo_hash);
      crx.installer = installer_;
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      if (num_calls_ == 1) {
        crx.version = base::Version("0.8");
      } else if (num_calls_ == 2) {
        crx.version = base::Version("1.0");
      } else {
        ADD_FAILURE();
      }

      std::move(callback).Run({crx});
    }

   private:
    friend class base::RefCountedThreadSafe<DataCallbackMock>;
    ~DataCallbackMock() = default;

    int num_calls_ = 0;
    scoped_refptr<VersionedTestInstaller> installer_ =
        base::MakeRefCounted<VersionedTestInstaller>();
  };
  auto data_callback_mock = MakeMockCallback<DataCallbackMock>();

  class MockUpdateChecker : public UpdateChecker {
   public:
    explicit MockUpdateChecker(int num_calls) : num_calls_(num_calls) {}

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      base::expected<ProtocolParser::Results, std::string> results;
      if (num_calls_ == 1) {
        results = ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"
                    }
                  ],
                  "out": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  },
                  "size": 54014
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      } else if (num_calls_ == 2) {
        context->get_available_space = base::BindRepeating(
            [](const base::FilePath&) -> int64_t { return 0; });

        results = ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "2.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.puff"
                    }
                  ],
                  "out": {
                    "sha256": "f2254da51fa2478a8ba90e58e1c28e24033ec7841015eebf1c82e31b957c44b2"
                  },
                  "size": 1680
                },
                {
                  "type": "puff",
                  "previous": {
                    "sha256": "8f5aa190311237cae00675af87ff457f278cd1a05895470ac5d46647d4a3c2ea"
                  },
                  "out": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                }
              ]
            },
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"
                    }
                  ],
                  "out": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  },
                  "size": 54409
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "c87d8742c3ff3d7a0cb6f3c91aa2fcf3dea63618086a7db1c5be5300e1d4d6b6"
                  }
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      } else {
        ADD_FAILURE();
      }
      EXPECT_TRUE(results.has_value()) << results.error();
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(context->components_to_check_for_updates.size(), 1u);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(update_check_callback), results.value(),
                         ErrorCategory::kNone, 0, 0));
    }

   private:
    const int num_calls_;
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 54014;
        download_metrics.total_bytes = 54014;
        download_metrics.download_time_ms = 2000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes / 2,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(2u, ping_data.size());
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[0].id);
      EXPECT_EQ(base::Version("0.8"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(0, ping_data[0].error_code);
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[1].id);
      EXPECT_EQ(base::Version("1.0"), ping_data[1].previous_version);
      EXPECT_EQ(base::Version("2.0"), ping_data[1].next_version);
      EXPECT_EQ(1, static_cast<int>(ping_data[1].error_category));
      EXPECT_EQ(static_cast<int>(CrxDownloaderError::DISK_FULL),
                ping_data[1].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdated;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  const std::vector<std::string> ids = {"ihfokbkgjpifnbbojhneepfflplebdkc"};
  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();

    EXPECT_EQ(10u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
    EXPECT_EQ(ComponentState::kDownloading, items[2].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[2].id);
    EXPECT_EQ(ComponentState::kDownloading, items[3].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[3].id);
    EXPECT_EQ(ComponentState::kDownloading, items[4].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[4].id);
    EXPECT_EQ(ComponentState::kUpdating, items[5].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[5].id);
    EXPECT_EQ(ComponentState::kUpdating, items[6].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[6].id);
    EXPECT_EQ(ComponentState::kUpdating, items[7].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[7].id);
    EXPECT_EQ(ComponentState::kUpdating, items[8].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[8].id);
    EXPECT_EQ(ComponentState::kUpdated, items[9].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[9].id);

    std::vector samples = {-1, -1, -1, -1, -1, -1, -1, 50, 100, 100};
    EXPECT_EQ(items.size(), samples.size());
    for (size_t i = 0; i != items.size(); ++i) {
      EXPECT_EQ(items[i].install_progress, samples[i]);
    }
  }

  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();

    EXPECT_EQ(5u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
    EXPECT_EQ(ComponentState::kDownloading, items[2].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[2].id);
    EXPECT_EQ(ComponentState::kDownloading, items[3].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[3].id);
    EXPECT_EQ(ComponentState::kUpdateError, items[4].state);
    EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[4].id);
  }
}

struct SendPingTestCase {
  const int event_type;
  const int result;
  const std::optional<int> error_code;
  const int extra_code1;
  const std::optional<base::Version> previous_version;
  const std::optional<base::Version> next_version;
};

class SendPingTest : public ::testing::WithParamInterface<SendPingTestCase>,
                     public UpdateClientTest {};

INSTANTIATE_TEST_SUITE_P(SendPingTestCases,
                         SendPingTest,
                         ::testing::ValuesIn(std::vector<SendPingTestCase>{
                             // Install ping.
                             {protocol_request::kEventInstall, 1, 2, 3},

                             // Uninstall ping.
                             {protocol_request::kEventUninstall,
                              1,
                              {},
                              10,
                              base::Version("1.2.3.4")},
                         }));

TEST_P(SendPingTest, SendPingTestCases) {
  MockUpdateCheckerFactory<MockUpdateCheckerAlwaysFails>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    static scoped_refptr<CrxDownloader> Create(
        bool is_background_download,
        scoped_refptr<NetworkFetcherFactory> network_fetcher_factory) {
      return nullptr;
    }

    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      EXPECT_EQ(ping_data().size(), 1u);
      EXPECT_EQ(ping_data()[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(ping_data()[0].event_type, GetParam().event_type);
      EXPECT_EQ(ping_data()[0].event_result, GetParam().result);
      if (GetParam().error_code) {
        EXPECT_EQ(ping_data()[0].error_code, *GetParam().error_code);
      }
      EXPECT_EQ(ping_data()[0].extra_code1, GetParam().extra_code1);
      if (GetParam().previous_version) {
        EXPECT_EQ(ping_data()[0].previous_version,
                  *GetParam().previous_version);
      }
      if (GetParam().next_version) {
        EXPECT_EQ(ping_data()[0].next_version, *GetParam().next_version);
      }
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  CrxComponent crx;
  crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
  crx.name = "test_jebg";
  crx.version = GetParam().previous_version.value_or(base::Version("1.2.3.4"));
  update_client->SendPing(crx,
                          {.event_type = GetParam().event_type,
                           .result = GetParam().result,
                           .error_code = GetParam().error_code.value_or(0),
                           .extra_code1 = GetParam().extra_code1},
                          ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();
}

TEST_F(UpdateClientTest, RetryAfter) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    explicit MockUpdateChecker(int num_calls) : num_calls_(num_calls) {}

   private:
    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());

      EXPECT_LE(num_calls_, 3);

      int retry_after_sec(0);
      if (num_calls_ == 1) {
        // Throttle the next call.
        retry_after_sec = 60 * 60;  // 1 hour.
      }

      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";

      ProtocolParser::Results results;
      results.apps.push_back(result);

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, retry_after_sec));
    }

    const int num_calls_;
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
  }

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  {
    // The engine handles this Update call but responds with a valid
    // |retry_after_sec|, which causes subsequent calls to fail.
    base::RunLoop runloop;
    update_client->Update(ids, base::BindOnce(&DataCallbackMock::Callback), {},
                          false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();
  }
  {
    // This call will result in a completion callback invoked with
    // Error::ERROR_UPDATE_RETRY_LATER.
    base::RunLoop runloop;
    update_client->Update(ids, base::BindOnce(&DataCallbackMock::Callback), {},
                          false,
                          ExpectErrorThenQuit(runloop, Error::RETRY_LATER));
    runloop.Run();
  }
  {
    // The Install call is handled, and the throttling is reset due to
    // the value of |retry_after_sec| in the completion callback.
    base::RunLoop runloop;
    update_client->Install(std::string("jebgalgnebhfojomionfpkfelancnnkf"),
                           base::BindOnce(&DataCallbackMock::Callback), {},
                           ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();
  }
  {
    // This call succeeds.
    base::RunLoop runloop;
    update_client->Update(ids, base::BindOnce(&DataCallbackMock::Callback), {},
                          false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();
  }
}

// Tests the update check for two CRXs scenario. The first component supports
// the group policy to enable updates, and has its updates disabled. The second
// component has an update. The server does not honor the "updatedisabled"
// attribute and returns updates for both components. However, the update for
// the first component is not applied and the client responds with a
// (SERVICE_ERROR, UPDATE_DISABLED)
TEST_F(UpdateClientTest, TwoCrxUpdateOneUpdateDisabled) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx1;
      crx1.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx1.name = "test_jebg";
      crx1.pk_hash = base::ToVector(jebg_hash);
      crx1.version = base::Version("0.9");
      crx1.installer = base::MakeRefCounted<TestInstaller>();
      crx1.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      crx1.updates_enabled = false;

      CrxComponent crx2;
      crx2.app_id = "ihfokbkgjpifnbbojhneepfflplebdkc";
      crx2.name = "test_ihfo";
      crx2.pk_hash = base::ToVector(ihfo_hash);
      crx2.version = base::Version("0.8");
      crx2.installer = base::MakeRefCounted<TestInstaller>();
      crx2.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx1, crx2});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsTwoCrxUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 54014;
        download_metrics.total_bytes = 54014;
        download_metrics.download_time_ms = 2000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(2u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(4, static_cast<int>(ping_data[0].error_category));
      EXPECT_EQ(2, ping_data[0].error_code);
      EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", ping_data[1].id);
      EXPECT_EQ(base::Version("0.8"), ping_data[1].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[1].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[1].error_category));
      EXPECT_EQ(0, ping_data[1].error_code);
    }
  };

  // Disables updates for the components declaring support for the group policy.
  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdated;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf",
                                        "ihfokbkgjpifnbbojhneepfflplebdkc"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(9u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kChecking, items[1].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[1].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[4].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[4].id);
  EXPECT_EQ(ComponentState::kDownloading, items[5].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[5].id);
  EXPECT_EQ(ComponentState::kDownloading, items[6].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[6].id);
  EXPECT_EQ(ComponentState::kUpdating, items[7].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[7].id);
  EXPECT_EQ(ComponentState::kUpdated, items[8].state);
  EXPECT_EQ("ihfokbkgjpifnbbojhneepfflplebdkc", items[8].id);
}

// Tests all ping back events have the correct errorcode and extracode1 set in
// the case of a failed download with a valid http status code
TEST_F(UpdateClientTest, OneCrxUpdateDownloadTimeout) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::vector<std::optional<CrxComponent>> component = {crx};
      std::move(callback).Run({component});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      download_metrics.url = url;
      download_metrics.downloader = DownloadMetrics::kNone;
      download_metrics.error = 200;
      download_metrics.extra_code1 = -2147012894;
      download_metrics.downloaded_bytes = 1015 / 2;
      download_metrics.total_bytes = 1015;
      download_metrics.download_time_ms = 1000;

      base::FilePath path;
      EXPECT_TRUE(MakeTestFile(
          GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

      Result result;
      result.error = 200;
      result.extra_code1 = -2147012894;

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes / 2,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::ping_data();
      EXPECT_EQ(ping_data.size(), 2u);

      auto download_ping = ping_data[0];
      EXPECT_EQ(download_ping.event_type, 14);
      EXPECT_EQ(download_ping.event_result, 0);
      EXPECT_EQ(download_ping.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(download_ping.previous_version, base::Version("0.9"));
      EXPECT_EQ(download_ping.next_version, base::Version("1.0"));
      EXPECT_EQ(download_ping.error_category, ErrorCategory::kNone);
      EXPECT_EQ(download_ping.error_code, 200);
      EXPECT_EQ(download_ping.extra_code1, -2147012894);

      auto update_ping = ping_data[1];
      EXPECT_EQ(update_ping.event_type, 3);
      EXPECT_EQ(update_ping.event_result, 0);
      EXPECT_EQ(update_ping.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(update_ping.previous_version, base::Version("0.9"));
      EXPECT_EQ(update_ping.next_version, base::Version("1.0"));
      EXPECT_EQ(update_ping.error_category, ErrorCategory::kDownload);
      EXPECT_EQ(update_ping.error_code, 200);
      EXPECT_EQ(update_ping.extra_code1, -2147012894);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(2));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(1, static_cast<int>(item.error_category));
          EXPECT_EQ(200, item.error_code);
          EXPECT_EQ(-2147012894, item.extra_code1);
        });
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(5u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kDownloading, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
}

// Tests the scenario where the update check fails.
TEST_F(UpdateClientTest, OneCrxUpdateCheckFails) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(update_check_callback), std::nullopt,
                         ErrorCategory::kUpdateCheck, -1, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(5, static_cast<int>(item.error_category));
          EXPECT_EQ(-1, item.error_code);
          EXPECT_EQ(0, item.extra_code1);
        });
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::UPDATE_CHECK_ERROR));
  runloop_.Run();

  EXPECT_EQ(2u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
}

// Tests the scenario where the server responds with different values for
// application status.
TEST_F(UpdateClientTest, OneCrxErrorUnknownApp) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      std::vector<std::optional<CrxComponent>> component;
      {
        CrxComponent crx;
        crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
        crx.name = "test_jebg";
        crx.pk_hash = base::ToVector(jebg_hash);
        crx.version = base::Version("0.9");
        crx.installer = base::MakeRefCounted<TestInstaller>();
        crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
        component.push_back(crx);
      }
      {
        CrxComponent crx;
        crx.app_id = "abagagagagagagagagagagagagagagag";
        crx.name = "test_abag";
        crx.pk_hash = base::ToVector(abag_hash);
        crx.version = base::Version("0.1");
        crx.installer = base::MakeRefCounted<TestInstaller>();
        crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
        component.push_back(crx);
      }
      {
        CrxComponent crx;
        crx.app_id = "ihfokbkgjpifnbbojhneepfflplebdkc";
        crx.name = "test_ihfo";
        crx.pk_hash = base::ToVector(ihfo_hash);
        crx.version = base::Version("0.2");
        crx.installer = base::MakeRefCounted<TestInstaller>();
        crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
        component.push_back(crx);
      }
      {
        CrxComponent crx;
        crx.app_id = "gjpmebpgbhcamgdgjcmnjfhggjpgcimm";
        crx.name = "test_gjpm";
        crx.pk_hash = base::ToVector(gjpm_hash);
        crx.version = base::Version("0.3");
        crx.installer = base::MakeRefCounted<TestInstaller>();
        crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
        component.push_back(crx);
      }
      std::move(callback).Run(component);
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(4u, context->components_to_check_for_updates.size());

      const std::string update_response = R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "jebgalgnebhfojomionfpkfelancnnkf",
        "status": "error-unknownApplication"
      },
      {
        "appid": "abagagagagagagagagagagagagagagag",
        "status": "restricted"
      },
      {
        "appid": "ihfokbkgjpifnbbojhneepfflplebdkc",
        "status": "error-invalidAppId"
      },
      {
        "appid": "gjpmebpgbhcamgdgjcmnjfhggjpgcimm",
        "status": "error-foobarApp"
      }
    ]
  }
})";

      const auto parser = ProtocolHandlerFactoryJSON().CreateParser();
      EXPECT_TRUE(parser->Parse(update_response));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(update_check_callback), parser->results(),
                         ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(5, static_cast<int>(item.error_category));
          EXPECT_EQ(-10006, item.error_code);  // UNKNOWN_APPPLICATION.
          EXPECT_EQ(0, item.extra_code1);
        });
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "abagagagagagagagagagagagagagagag" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "abagagagagagagagagagagagagagagag" &&
                         item.state == ComponentState::kUpdateError;
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(5, static_cast<int>(item.error_category));
          EXPECT_EQ(-10007, item.error_code);  // RESTRICTED_APPLICATION.
          EXPECT_EQ(0, item.extra_code1);
        });
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "ihfokbkgjpifnbbojhneepfflplebdkc" &&
                         item.state == ComponentState::kUpdateError;
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(5, static_cast<int>(item.error_category));
          EXPECT_EQ(-10008, item.error_code);  // INVALID_APPID.
          EXPECT_EQ(0, item.extra_code1);
        });
  }
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "gjpmebpgbhcamgdgjcmnjfhggjpgcimm" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "gjpmebpgbhcamgdgjcmnjfhggjpgcimm" &&
                         item.state == ComponentState::kUpdateError;
                })))
        .WillOnce([](const CrxUpdateItem& item) {
          EXPECT_EQ(ComponentState::kUpdateError, item.state);
          EXPECT_EQ(5, static_cast<int>(item.error_category));
          EXPECT_EQ(-10016, item.error_code);  // UNKNOWN_ERROR.
          EXPECT_EQ(0, item.extra_code1);
        });
  }

  const std::vector<std::string> ids = {
      "jebgalgnebhfojomionfpkfelancnnkf", "abagagagagagagagagagagagagagagag",
      "ihfokbkgjpifnbbojhneepfflplebdkc", "gjpmebpgbhcamgdgjcmnjfhggjpgcimm"};
  update_client->Update(ids, base::BindOnce(&DataCallbackMock::Callback), {},
                        true, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();
}

// Tests that a run action in invoked in the CRX install scenario.
TEST_F(UpdateClientTest, ActionRun_Install) {
  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      context->get_available_space = base::BindRepeating(
          [](const base::FilePath&) -> int64_t { return 200000; });
      base::expected<ProtocolParser::Results, std::string> results =
          ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "gjpmebpgbhcamgdgjcmnjfhggjpgcimm",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "operations": [
                {
                  "type": "download",
                  "urls": [
                    {
                      "url": "http://localhost/download/runaction_test_win.crx3"
                    }
                  ],
                  "out": {
                    "sha256": "89290a0d2ff21ca5b45e109c6cc859ab5fe294e19c102d54acd321429c372cea"
                  },
                  "size": 48141
                },
                {
                  "type": "crx3",
                  "in": {
                    "sha256": "89290a0d2ff21ca5b45e109c6cc859ab5fe294e19c102d54acd321429c372cea"
                  }
                },
                {
                  "type": "run",
                  "path": "ChromeRecovery.crx3"
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      EXPECT_TRUE(results.has_value()) << results.error();
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(context->components_to_check_for_updates.size(), 1u);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(update_check_callback), results.value(),
                         ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/runaction_test_win.crx3") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 48141;
        download_metrics.total_bytes = 48141;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(
            MakeTestFile(GetTestFilePath("runaction_test_win.crx3"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      EXPECT_EQ(4u, events().size());

      /*
      "<event eventtype="14" eventresult="1" downloader="unknown"
      url="http://localhost/download/runaction_test_win.crx3"
      downloaded=48141 total=48141 download_time_ms="1000"
      previousversion="0.0" nextversion="1.0"/>
      */
      const base::Value::Dict& event0 = events()[0];
      EXPECT_EQ(14, event0.FindInt("eventtype"));
      EXPECT_EQ(1, event0.FindInt("eventresult"));
      EXPECT_EQ("unknown", CHECK_DEREF(event0.FindString("downloader")));
      EXPECT_EQ("http://localhost/download/runaction_test_win.crx3",
                CHECK_DEREF(event0.FindString("url")));
      EXPECT_EQ(48141, event0.FindDouble("downloaded"));
      EXPECT_EQ(48141, event0.FindDouble("total"));
      EXPECT_EQ(1000, event0.FindDouble("download_time_ms"));
      EXPECT_EQ("0.0", CHECK_DEREF(event0.FindString("previousversion")));
      EXPECT_EQ("1.0", CHECK_DEREF(event0.FindString("nextversion")));

      // <event eventtype="63" eventresult="1" previousversion="0.0"
      // nextversion="1.0"/>
      const base::Value::Dict& event1 = events()[1];
      EXPECT_EQ(63, event1.FindInt("eventtype"));
      EXPECT_EQ(1, event1.FindInt("eventresult"));
      EXPECT_EQ("0.0", CHECK_DEREF(event1.FindString("previousversion")));
      EXPECT_EQ("1.0", CHECK_DEREF(event1.FindString("nextversion")));

      // <event eventtype="42" eventresult="1" errorcode="1877345072"/>
      const base::Value::Dict& event2 = events()[2];
      EXPECT_EQ(42, event2.FindInt("eventtype"));
      EXPECT_EQ(1, event2.FindInt("eventresult"));
      EXPECT_EQ(1877345072, event2.FindInt("errorcode"));

      // <event eventtype="2" eventresult="1" previousversion="0.0"
      // nextversion="1.0"/>
      const base::Value::Dict& event3 = events()[3];
      EXPECT_EQ(2, event3.FindInt("eventtype"));
      EXPECT_EQ(1, event3.FindInt("eventresult"));
      EXPECT_EQ("0.0", CHECK_DEREF(event3.FindString("previousversion")));
      EXPECT_EQ("1.0", CHECK_DEREF(event3.FindString("nextversion")));
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  update_client->Install(
      std::string("gjpmebpgbhcamgdgjcmnjfhggjpgcimm"),
      base::BindOnce(
          [](const std::vector<std::string>& ids,
             base::OnceCallback<void(
                 const std::vector<std::optional<CrxComponent>>&)> callback) {
            auto action_handler = base::MakeRefCounted<MockActionHandler>();
            EXPECT_CALL(*action_handler, Handle(_, _, _))
                .WillOnce([](const base::FilePath& action,
                             const std::string& session_id,
                             ActionHandler::Callback callback) {
                  EXPECT_EQ("ChromeRecovery.crx3",
                            action.BaseName().AsUTF8Unsafe());
                  EXPECT_TRUE(!session_id.empty());
                  std::move(callback).Run(true, 1877345072, 0);
                });

            CrxComponent crx;
            crx.app_id = "gjpmebpgbhcamgdgjcmnjfhggjpgcimm";
            crx.name = "test_gjpm";
            crx.pk_hash = base::ToVector(gjpm_hash);
            crx.version = base::Version("0.0");
            crx.installer = base::MakeRefCounted<VersionedTestInstaller>();
            crx.action_handler = action_handler;
            crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
            std::move(callback).Run({crx});
          }),
      {}, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();
}

// Tests that a run action is invoked in an update scenario when there was
// no update.
TEST_F(UpdateClientTest, ActionRun_NoUpdate) {
  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsActionRunNoUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      auto ping_data = MockPingManagerImpl::ping_data();
      EXPECT_EQ(ping_data.size(), 2u);

      // "<event eventtype="42" eventresult="1" errorcode="1877345072"/>"
      auto download = ping_data[0];
      EXPECT_EQ(download.event_type, protocol_request::kEventAction);
      EXPECT_EQ(download.event_result, protocol_request::kEventResultSuccess);
      EXPECT_EQ(download.error_code, 1877345072);

      // "<event eventtype="3" eventresult="1"/>"
      auto install = ping_data[1];
      EXPECT_EQ(install.event_type, protocol_request::kEventUpdate);
      EXPECT_EQ(install.event_result, protocol_request::kEventResultSuccess);
    }
  };

  // Unpack the CRX to mock an existing install to be updated. The action to
  // run is going to be resolved relative to this directory.
  base::FilePath unpack_path;
  {
    base::RunLoop runloop;
    base::OnceClosure quit_closure = runloop.QuitClosure();

    Unpacker::Unpack(
        "gjpmebpgbhcamgdgjcmnjfhggjpgcimm", "UpdateClientTest",
        std::vector<uint8_t>(std::begin(gjpm_hash), std::end(gjpm_hash)),
        GetTestFilePath("runaction_test_win.crx3"),
        base::MakeRefCounted<UnzipChromiumFactory>(
            base::BindRepeating(&unzip::LaunchInProcessUnzipper))
            ->Create(),
        crx_file::VerifierFormat::CRX3,
        base::BindOnce(
            [](base::FilePath* unpack_path, base::OnceClosure quit_closure,
               const Unpacker::Result& result) {
              EXPECT_EQ(UnpackerError::kNone, result.error);
              EXPECT_EQ(0, result.extended_error);
              *unpack_path = result.unpack_path;
              std::move(quit_closure).Run();
            },
            &unpack_path, runloop.QuitClosure()));

    runloop.Run();
  }

  EXPECT_FALSE(unpack_path.empty());
  EXPECT_TRUE(base::DirectoryExists(unpack_path));
  std::optional<int64_t> file_size = base::GetFileSize(
      unpack_path.Append(FILE_PATH_LITERAL("ChromeRecovery.crx3")));
  EXPECT_TRUE(file_size.has_value());
  EXPECT_EQ(44582, file_size.value());

  base::ScopedTempDir unpack_path_owner;
  EXPECT_TRUE(unpack_path_owner.Set(unpack_path));

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  const std::vector<std::string> ids = {"gjpmebpgbhcamgdgjcmnjfhggjpgcimm"};
  update_client->Update(
      ids,
      base::BindOnce(
          [](const base::FilePath& unpack_path,
             const std::vector<std::string>& ids,
             base::OnceCallback<void(
                 const std::vector<std::optional<CrxComponent>>&)> callback) {
            auto action_handler = base::MakeRefCounted<MockActionHandler>();
            EXPECT_CALL(*action_handler, Handle(_, _, _))
                .WillOnce([](const base::FilePath& action,
                             const std::string& session_id,
                             ActionHandler::Callback callback) {
                  EXPECT_EQ("ChromeRecovery.crx3",
                            action.BaseName().AsUTF8Unsafe());
                  EXPECT_TRUE(!session_id.empty());
                  std::move(callback).Run(true, 1877345072, 0);
                });

            CrxComponent crx;
            crx.app_id = "gjpmebpgbhcamgdgjcmnjfhggjpgcimm";
            crx.name = "test_gjpm";
            crx.pk_hash = base::ToVector(gjpm_hash);
            crx.version = base::Version("1.0");
            crx.installer =
                base::MakeRefCounted<ReadOnlyTestInstaller>(unpack_path);
            crx.action_handler = action_handler;
            crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
            std::move(callback).Run({crx});
          },
          unpack_path),
      {}, false, ExpectErrorThenQuit(runloop_, Error::NONE));

  runloop_.Run();
}

// Tests that custom response attributes are visible to observers.
TEST_F(UpdateClientTest, CustomAttributeNoUpdate) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::vector<std::optional<CrxComponent>> component = {crx};
      std::move(callback).Run(component);
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      auto& component = context->components.at(id);

      EXPECT_TRUE(component->is_foreground());

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";
      result.custom_attributes["_example"] = "example_value";

      ProtocolParser::Results results;
      results.apps.push_back(result);

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  EXPECT_CALL(observer, OnEvent(_))
      .WillRepeatedly([](const CrxUpdateItem& item) {
        if (item.state == ComponentState::kUpToDate) {
          ASSERT_TRUE(item.custom_updatecheck_data.contains("_example"));
          EXPECT_EQ("example_value",
                    item.custom_updatecheck_data.at("_example"));
        }
      });

  update_client->Update({"jebgalgnebhfojomionfpkfelancnnkf"},
                        base::BindOnce(&DataCallbackMock::Callback), {}, true,
                        ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();
}

// Tests the scenario where `CrxDataCallback` returns a vector whose elements
// don't include a value for one of the component ids specified by the `ids`
// parameter of the `UpdateClient::Update` function. Expects the completion
// callback to include a specific error, and no other events and pings be
// generated, since the update engine rejects the UpdateClient::Update call.
TEST_F(UpdateClientTest, BadCrxDataCallback) {
  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          UpdateChecker::Factory{});

  MockObserver observer(update_client);

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf",
                                        "gjpmebpgbhcamgdgjcmnjfhggjpgcimm"};
  // The `CrxDataCallback` argument only returns a value for the first
  // component id. This means that its result is ill formed, and the `Update`
  // call completes with an error.
  update_client->Update(
      ids,
      base::BindOnce(
          [](const std::vector<std::string>& ids,
             base::OnceCallback<void(
                 const std::vector<std::optional<CrxComponent>>&)> callback) {
            EXPECT_EQ(ids.size(), size_t{2});
            std::move(callback).Run({std::nullopt});
          }),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver), true,
      ExpectErrorThenQuit(runloop_, Error::BAD_CRX_DATA_CALLBACK));
  runloop_.Run();

  EXPECT_TRUE(items.empty());
}

// Tests cancellation of an install before the task is run.
TEST_F(UpdateClientTest, CancelInstallBeforeTaskStart) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.0");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxInstall>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/jebgalgnebhfojomionfpkfelancnnkf.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 1015;
        download_metrics.total_bytes = 1015;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::ping_data();
      EXPECT_EQ(0u, ping_data.size());
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  update_client
      ->Install(
          std::string("jebgalgnebhfojomionfpkfelancnnkf"),
          base::BindOnce(&DataCallbackMock::Callback),
          base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
          ExpectErrorThenQuit(runloop_, Error::UPDATE_CANCELED))
      .Run();
  runloop_.Run();
  EXPECT_EQ(0u, items.size());
}

// Tests cancellation of an install before the component installer runs.
TEST_F(UpdateClientTest, CancelInstallBeforeInstall) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.0");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxInstall>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/jebgalgnebhfojomionfpkfelancnnkf.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 1015;
        download_metrics.total_bytes = 1015;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(1u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.0"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(ErrorCategory::kService, ping_data[0].error_category);
      EXPECT_EQ(static_cast<int>(ServiceError::CANCELLED),
                ping_data[0].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  base::RepeatingClosure cancel;

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(AtLeast(1))
        .WillRepeatedly([&cancel] { cancel.Run(); });
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  cancel = update_client->Install(
      std::string("jebgalgnebhfojomionfpkfelancnnkf"),
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(5u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kDownloading, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
  EXPECT_EQ(ComponentState::kDownloading, items[3].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[4].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
}

// Tests cancellation of an install before the download.
TEST_F(UpdateClientTest, CancelInstallBeforeDownload) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.version = base::Version("0.0");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxInstall>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      base::FilePath path;
      Result result;
      if (url.GetPath() == "/download/jebgalgnebhfojomionfpkfelancnnkf.crx") {
        download_metrics.url = url;
        download_metrics.downloader = DownloadMetrics::kNone;
        download_metrics.error = 0;
        download_metrics.downloaded_bytes = 1015;
        download_metrics.total_bytes = 1015;
        download_metrics.download_time_ms = 1000;

        EXPECT_TRUE(MakeTestFile(
            GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

        result.error = 0;
        result.response = path;
      } else {
        ADD_FAILURE();
      }

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(1u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.0"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(ErrorCategory::kService, ping_data[0].error_category);
      EXPECT_EQ(static_cast<int>(ServiceError::CANCELLED),
                ping_data[0].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  base::RepeatingClosure cancel;

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })))
        .WillOnce([&cancel] { cancel.Run(); });
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  cancel = update_client->Install(
      std::string("jebgalgnebhfojomionfpkfelancnnkf"),
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
}

TEST_F(UpdateClientTest, CheckForUpdate_NoUpdate) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      auto& component = context->components.at(id);

      EXPECT_TRUE(component->is_foreground());

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";

      ProtocolParser::Results results;
      results.apps.push_back(result);

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
  }
  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
  update_client->CheckForUpdate(
      id, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/true, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();
  EXPECT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0].state, ComponentState::kChecking);
  EXPECT_EQ(items[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[1].state, ComponentState::kUpToDate);
  EXPECT_EQ(items[1].id, "jebgalgnebhfojomionfpkfelancnnkf");
}

TEST_F(UpdateClientTest, CheckForUpdate_UpdateAvailable) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const std::vector<PingData> ping_data =
          MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(ping_data.size(), 1u);
      EXPECT_EQ(ping_data[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(ping_data[0].previous_version, base::Version("0.9"));
      EXPECT_EQ(ping_data[0].next_version, base::Version("1.0"));
      EXPECT_EQ(ping_data[0].error_category, ErrorCategory::kService);
      EXPECT_EQ(ping_data[0].error_code,
                static_cast<int>(ServiceError::CHECK_FOR_UPDATE_ONLY));
      EXPECT_EQ(ping_data[0].extra_code1, 0);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
  }
  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
  update_client->CheckForUpdate(
      id, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();
  EXPECT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0].state, ComponentState::kChecking);
  EXPECT_EQ(items[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[1].state, ComponentState::kCanUpdate);
  EXPECT_EQ(items[1].id, "jebgalgnebhfojomionfpkfelancnnkf");
}

TEST_F(UpdateClientTest, CheckForUpdate_QueueChecks) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      auto& component = context->components.at(id);

      EXPECT_TRUE(component->is_foreground());

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";

      ProtocolParser::Results results;
      results.apps.push_back(result);

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
  }
  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  // Do two `CheckForUpdate` calls, expect the calls to be done in sequence.
  base::RepeatingClosure barrier_quit_closure =
      BarrierClosure(2, runloop_.QuitClosure());
  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
  update_client->CheckForUpdate(
      id, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/true,
      ExpectErrorThenQuit(barrier_quit_closure, Error::NONE));
  update_client->CheckForUpdate(
      id, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/true,
      ExpectErrorThenQuit(barrier_quit_closure, Error::NONE));
  EXPECT_TRUE(update_client->IsUpdating(id));
  runloop_.Run();
  EXPECT_EQ(items.size(), 4u);
  EXPECT_EQ(items[0].state, ComponentState::kChecking);
  EXPECT_EQ(items[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[1].state, ComponentState::kUpToDate);
  EXPECT_EQ(items[1].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[2].state, ComponentState::kChecking);
  EXPECT_EQ(items[2].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[3].state, ComponentState::kUpToDate);
  EXPECT_EQ(items[3].id, "jebgalgnebhfojomionfpkfelancnnkf");
}

TEST_F(UpdateClientTest, CheckForUpdate_Stop) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      std::move(callback).Run({crx});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(1u, context->components_to_check_for_updates.size());
      const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
      EXPECT_EQ(id, context->components_to_check_for_updates.front());
      EXPECT_EQ(1u, context->components.count(id));

      auto& component = context->components.at(id);

      EXPECT_TRUE(component->is_foreground());

      ProtocolParser::App result;
      result.app_id = id;
      result.status = "noupdate";

      ProtocolParser::Results results;
      results.apps.push_back(result);

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(update_check_callback), results,
                                    ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpToDate;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  // Do two `CheckForUpdate` calls, expect the second call to be cancelled,
  // because `Stop` cancels the queued up subsequent call.
  base::RepeatingClosure barrier_quit_closure =
      BarrierClosure(2, runloop_.QuitClosure());
  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
  update_client->CheckForUpdate(
      id, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/true,
      ExpectErrorThenQuit(barrier_quit_closure, Error::NONE));
  update_client->CheckForUpdate(
      id, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/true,
      ExpectErrorThenQuit(barrier_quit_closure, Error::UPDATE_CANCELED));
  update_client->Stop();
  EXPECT_TRUE(update_client->IsUpdating(id));
  runloop_.Run();
  EXPECT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0].state, ComponentState::kChecking);
  EXPECT_EQ(items[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[1].state, ComponentState::kUpToDate);
  EXPECT_EQ(items[1].id, "jebgalgnebhfojomionfpkfelancnnkf");
}

TEST_F(UpdateClientTest, CheckForUpdate_Errors) {
  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {}
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override { EXPECT_TRUE(ping_data().empty()); }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }
  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  // Tests some error cases when arguments are incorrect.
  base::RepeatingClosure barrier_quit_closure =
      BarrierClosure(2, runloop_.QuitClosure());
  const std::string id = "jebgalgnebhfojomionfpkfelancnnkf";
  update_client->CheckForUpdate(
      id,
      base::BindOnce(
          [](const std::vector<std::string>& /*ids*/,
             base::OnceCallback<void(
                 const std::vector<std::optional<CrxComponent>>&)> callback) {
            std::move(callback).Run({});
          }),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/true,
      ExpectErrorThenQuit(barrier_quit_closure, Error::BAD_CRX_DATA_CALLBACK));
  update_client->CheckForUpdate(
      id,
      base::BindLambdaForTesting(
          [&id](
              const std::vector<std::string>& ids,
              base::OnceCallback<void(
                  const std::vector<std::optional<CrxComponent>>&)> callback) {
            EXPECT_EQ(ids.size(), 1u);
            EXPECT_EQ(id, ids[0]);
            std::move(callback).Run({std::nullopt});
          }),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      /*is_foreground=*/true,
      ExpectErrorThenQuit(barrier_quit_closure, Error::NONE));
  EXPECT_TRUE(update_client->IsUpdating(id));
  runloop_.Run();
  EXPECT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].state, ComponentState::kUpdateError);
  EXPECT_EQ(items[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[0].error_code, static_cast<int>(Error::CRX_NOT_FOUND));
}

// Tests `CheckForUpdate` when the updates are disabled but the server ignores
// "updatedisabled" attribute and returns on update. In this case, the client
// reports an error (SERVICE_ERROR, UPDATE_DISABLED) and pings.
TEST_F(UpdateClientTest, UpdateCheck_UpdateDisabled) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.installer = base::MakeRefCounted<TestInstaller>();
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;
      crx.updates_enabled = false;
      std::move(callback).Run({crx});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const std::vector<PingData>& ping_data =
          MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(ping_data.size(), 1u);
      EXPECT_EQ(ping_data[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(ping_data[0].previous_version, base::Version("0.9"));
      EXPECT_EQ(ping_data[0].next_version, base::Version("1.0"));
      EXPECT_EQ(ping_data[0].error_category, ErrorCategory::kService);
      EXPECT_EQ(ping_data[0].error_code,
                static_cast<int>(ServiceError::UPDATE_DISABLED));
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }
  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  update_client->CheckForUpdate(
      "jebgalgnebhfojomionfpkfelancnnkf",
      base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();
  EXPECT_EQ(items.size(), 3u);
  EXPECT_EQ(items[0].state, ComponentState::kChecking);
  EXPECT_EQ(items[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[1].state, ComponentState::kCanUpdate);
  EXPECT_EQ(items[1].id, "jebgalgnebhfojomionfpkfelancnnkf");
  EXPECT_EQ(items[2].state, ComponentState::kUpdateError);
  EXPECT_EQ(items[2].id, "jebgalgnebhfojomionfpkfelancnnkf");
}

// Tests the cached update scenario for one CRX to validate that the file is
// cached if an install error occurs and re-used when the update is retried.
TEST_F(UpdateClientTest, OneCrxCachedUpdate) {
  class DataCallbackMock : public base::RefCountedThreadSafe<DataCallbackMock> {
   public:
    void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      ++num_calls_;

      CrxComponent crx;
      crx.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx.name = "test_jebg";
      crx.pk_hash = base::ToVector(jebg_hash);
      crx.version = base::Version("0.9");
      crx.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      scoped_refptr<TestInstaller> installer =
          base::MakeRefCounted<TestInstaller>();
      if (num_calls_ == 1) {
        installer->set_install_error(InstallError::GENERIC_ERROR);
        installer->set_installer_progress_samples({-1, 25});
      } else if (num_calls_ == 2) {
        installer->set_installer_progress_samples({-1, 50, 100});
      } else {
        ADD_FAILURE();
      }
      crx.installer = installer;

      std::move(callback).Run({crx});
    }

   private:
    friend class base::RefCountedThreadSafe<DataCallbackMock>;
    ~DataCallbackMock() = default;

    int num_calls_ = 0;
  };
  auto data_callback_mock = MakeMockCallback<DataCallbackMock>();

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsOneCrxUpdate>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      DownloadMetrics download_metrics;
      download_metrics.url = url;
      download_metrics.downloader = DownloadMetrics::kNone;
      download_metrics.error = 0;
      download_metrics.downloaded_bytes = 54014;
      download_metrics.total_bytes = 54014;
      download_metrics.download_time_ms = 2000;

      base::FilePath path;
      EXPECT_TRUE(MakeTestFile(
          GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"), &path));

      Result result;
      result.error = 0;
      result.response = path;

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadProgress,
                                    base::Unretained(this),
                                    download_metrics.downloaded_bytes,
                                    download_metrics.total_bytes));

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockCrxDownloader::OnDownloadComplete,
                                    base::Unretained(this), true, result,
                                    download_metrics));
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::terminal_ping_data();
      EXPECT_EQ(2u, ping_data.size());
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[0].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[0].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[0].next_version);
      EXPECT_EQ(ping_data[0].error_category, ErrorCategory::kInstall);
      EXPECT_EQ(9, ping_data[0].error_code);  // GENERIC_ERROR.
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[1].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[1].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[1].next_version);
      EXPECT_EQ(0, static_cast<int>(ping_data[1].error_category));
      EXPECT_EQ(0, ping_data[1].error_code);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kDownloading;
                })))
        .Times(2);
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdating;
                })))
        .Times(2);
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));

    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdating;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdating;
                })))
        .Times(3);
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdated;
                })));
  }

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();

    EXPECT_EQ(8u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
    EXPECT_EQ(ComponentState::kDownloading, items[2].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
    EXPECT_EQ(ComponentState::kDownloading, items[3].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
    EXPECT_EQ(ComponentState::kUpdating, items[4].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
    EXPECT_EQ(ComponentState::kUpdating, items[5].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);
    EXPECT_EQ(ComponentState::kUpdating, items[6].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[6].id);
    EXPECT_EQ(ComponentState::kUpdateError, items[7].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[7].id);

    std::vector samples = {-1, -1, -1, -1, -1, -1, 25, 25};
    EXPECT_EQ(items.size(), samples.size());
    for (size_t i = 0; i != items.size(); ++i) {
      EXPECT_EQ(items[i].install_progress, samples[i]);
    }
  }

  {
    std::vector<CrxUpdateItem> items;
    auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
    EXPECT_CALL(*receiver, Receive(_))
        .WillRepeatedly(
            [&items](const CrxUpdateItem& item) { items.push_back(item); });

    base::RunLoop runloop;
    update_client->Update(
        ids, data_callback_mock,
        base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
        false, ExpectErrorThenQuit(runloop, Error::NONE));
    runloop.Run();

    EXPECT_EQ(7u, items.size());
    EXPECT_EQ(ComponentState::kChecking, items[0].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
    EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
    EXPECT_EQ(ComponentState::kUpdating, items[2].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
    EXPECT_EQ(ComponentState::kUpdating, items[3].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[3].id);
    EXPECT_EQ(ComponentState::kUpdating, items[4].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[4].id);
    EXPECT_EQ(ComponentState::kUpdating, items[5].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[5].id);
    EXPECT_EQ(ComponentState::kUpdated, items[6].state);
    EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[6].id);

    std::vector samples = {-1, -1, -1, -1, 50, 100, 100};
    EXPECT_EQ(items.size(), samples.size());
    for (size_t i = 0; i != items.size(); ++i) {
      EXPECT_EQ(items[i].install_progress, samples[i]);
    }
  }
}

TEST_F(UpdateClientTest, UnsupportedOperationType) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx1;
      crx1.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx1.name = "test_jebg";
      crx1.pk_hash = base::ToVector(jebg_hash);
      crx1.version = base::Version("0.9");
      crx1.installer = base::MakeRefCounted<TestInstaller>();
      crx1.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx1});
    }
  };

  MockUpdateCheckerFactory<
      MockUpdateCheckerImpl<UpdateCheckerOptionsUnsupportedOperationType>>
      mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE() << "Intentionally forcing download to fail here.";
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::ping_data();
      EXPECT_EQ(2u, ping_data.size());

      // unsupported operation event
      EXPECT_EQ(ping_data[0].id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(ping_data[0].previous_version, base::Version("0.9"));
      EXPECT_EQ(ping_data[0].next_version, base::Version("1.0"));
      EXPECT_EQ(ping_data[0].error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(ping_data[0].event_type, protocol_request::kEventUnknown);

      // update check
      EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", ping_data[1].id);
      EXPECT_EQ(base::Version("0.9"), ping_data[1].previous_version);
      EXPECT_EQ(base::Version("1.0"), ping_data[1].next_version);
      EXPECT_EQ(ping_data[1].error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(ping_data[1].event_type, protocol_request::kEventUpdate);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
}

TEST_F(UpdateClientTest,
       AllPipelinesContainingOperationsWithInvalidAttributesNoUpdate) {
  class DataCallbackMock {
   public:
    static void Callback(
        const std::vector<std::string>& ids,
        base::OnceCallback<
            void(const std::vector<std::optional<CrxComponent>>&)> callback) {
      CrxComponent crx1;
      crx1.app_id = "jebgalgnebhfojomionfpkfelancnnkf";
      crx1.name = "test_jebg";
      crx1.pk_hash = base::ToVector(jebg_hash);
      crx1.version = base::Version("0.9");
      crx1.installer = base::MakeRefCounted<TestInstaller>();
      crx1.crx_format_requirement = crx_file::VerifierFormat::CRX3;

      std::move(callback).Run({crx1});
    }
  };

  class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker() = default;

    void CheckForUpdates(
        scoped_refptr<UpdateContext> context,
        const base::flat_map<std::string, std::string>& additional_attributes,
        UpdateCheckCallback update_check_callback) override {
      context->get_available_space = base::BindRepeating(
          [](const base::FilePath&) -> int64_t { return 0; });
      // The puff operation is missing previous hash, so it fails.
      base::expected<ProtocolParser::Results, std::string> results =
          ProtocolParserJSON::ParseJSON(R"()]}'
{
  "response": {
    "protocol": "4.0",
    "apps": [
      {
        "appid": "jebgalgnebhfojomionfpkfelancnnkf",
        "status": "ok",
        "updatecheck": {
          "status": "ok",
          "nextversion": "1.0",
          "pipelines": [
            {
              "pipeline_id": "download_missing_urls",
              "operations": [
                {
                  "type": "download",
                  "size": 10,
                  "out": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            },
            {
              "pipeline_id": "download_missing_out",
              "operations": [
                {
                  "type": "download",
                  "size": 10,
                  "urls": {
                    "url": "http://does.not.matter.com/file.crx"
                  }
                }
              ]
            },
            {
              "pipeline_id": "download_invalid_size",
              "operations": [
                {
                  "type": "download",
                  "size": -10,
                  "urls": {
                    "url": "http://does.not.matter.com/file.crx"
                  },
                  "out": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            },
            {
              "pipeline_id": "puff_missing_prev",
              "operations": [
                {
                  "type": "puff",
                  "out": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            },
            {
              "pipeline_id": "zucc_missing_prev",
              "operations": [
                {
                  "type": "zucc",
                  "out": {
                    "sha256": "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498"
                  }
                }
              ]
            },
            {
              "pipeline_id": "crx3_missing_in",
              "operations": [
                {
                  "type": "crx3"
                }
              ]
            }
          ]
        }
      }
    ]
  }
})");
      EXPECT_TRUE(results.has_value()) << results.error();
      EXPECT_FALSE(context->session_id.empty());
      EXPECT_EQ(context->components_to_check_for_updates.size(), 1u);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(update_check_callback), results.value(),
                         ErrorCategory::kNone, 0, 0));
    }
  };
  MockUpdateCheckerFactory<MockUpdateChecker> mock_update_checker_factory;

  class MockCrxDownloader : public CrxDownloader {
   public:
    MockCrxDownloader() = default;

   private:
    ~MockCrxDownloader() override = default;

    base::OnceClosure DoStartDownload(const GURL& url) override {
      ADD_FAILURE();
      return base::DoNothing();
    }
  };

  class MockPingManager : public MockPingManagerImpl {
   public:
    explicit MockPingManager(scoped_refptr<Configurator> config)
        : MockPingManagerImpl(config) {}

   protected:
    ~MockPingManager() override {
      const auto ping_data = MockPingManagerImpl::ping_data();
      EXPECT_EQ(ping_data.size(), 7u);

      // Download failures:
      auto noUrls = ping_data[0];
      EXPECT_EQ(noUrls.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(noUrls.pipeline_id, "download_missing_urls");
      EXPECT_EQ(noUrls.error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(noUrls.event_type, protocol_request::kEventDownload);

      auto noOutHash = ping_data[1];
      EXPECT_EQ(noOutHash.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(noOutHash.pipeline_id, "download_missing_out");
      EXPECT_EQ(noOutHash.error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(noOutHash.event_type, protocol_request::kEventDownload);

      auto badSize = ping_data[2];
      EXPECT_EQ(badSize.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(badSize.pipeline_id, "download_invalid_size");
      EXPECT_EQ(badSize.error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(badSize.event_type, protocol_request::kEventDownload);

      auto puff = ping_data[3];
      EXPECT_EQ(puff.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(puff.pipeline_id, "puff_missing_prev");
      EXPECT_EQ(puff.error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(puff.event_type, protocol_request::kEventPuff);

      auto zucc = ping_data[4];
      EXPECT_EQ(zucc.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(zucc.pipeline_id, "zucc_missing_prev");
      EXPECT_EQ(zucc.error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(zucc.event_type, protocol_request::kEventZucchini);

      auto crx3 = ping_data[5];
      EXPECT_EQ(crx3.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_EQ(crx3.pipeline_id, "crx3_missing_in");
      EXPECT_EQ(crx3.error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(crx3.event_type, protocol_request::kEventCrx3);

      auto update = ping_data[6];
      EXPECT_EQ(update.id, "jebgalgnebhfojomionfpkfelancnnkf");
      EXPECT_TRUE(update.pipeline_id.empty());
      EXPECT_EQ(update.error_category, ErrorCategory::kUpdateCheck);
      EXPECT_EQ(update.event_type, protocol_request::kEventUpdate);
    }
  };

  SetMockCrxDownloader<MockCrxDownloader>();
  scoped_refptr<UpdateClient> update_client =
      base::MakeRefCounted<UpdateClientImpl>(
          config(), base::MakeRefCounted<MockPingManager>(config()),
          mock_update_checker_factory.GetFactory());

  MockObserver observer(update_client);
  {
    InSequence seq;
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kChecking;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kCanUpdate;
                })));
    EXPECT_CALL(observer, OnEvent(Truly([](const CrxUpdateItem& item) {
                  return item.id == "jebgalgnebhfojomionfpkfelancnnkf" &&
                         item.state == ComponentState::kUpdateError;
                })));
  }

  std::vector<CrxUpdateItem> items;
  auto receiver = base::MakeRefCounted<MockCrxStateChangeReceiver>();
  EXPECT_CALL(*receiver, Receive(_))
      .WillRepeatedly(
          [&items](const CrxUpdateItem& item) { items.push_back(item); });

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  update_client->Update(
      ids, base::BindOnce(&DataCallbackMock::Callback),
      base::BindRepeating(&MockCrxStateChangeReceiver::Receive, receiver),
      false, ExpectErrorThenQuit(runloop_, Error::NONE));
  runloop_.Run();

  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(ComponentState::kChecking, items[0].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[0].id);
  EXPECT_EQ(ComponentState::kCanUpdate, items[1].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[1].id);
  EXPECT_EQ(ComponentState::kUpdateError, items[2].state);
  EXPECT_EQ("jebgalgnebhfojomionfpkfelancnnkf", items[2].id);
}

}  // namespace update_client
