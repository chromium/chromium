// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/cdm_storage_adapter.h"

#include <utility>

#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/components/cdm_factory_daemon/mojom/cdm_storage.mojom.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using DaemonStorage = chromeos::cdm::mojom::CdmStorage;
using BrowserStorage = media::mojom::CdmStorage;
using media::mojom::CdmFile;
using testing::_;

namespace chromeos {

namespace {

constexpr char kFakeFilename[] = "filename";

class MockBrowserStorage : public BrowserStorage {
 public:
  MockBrowserStorage(mojo::PendingReceiver<BrowserStorage> receiver) {
    receiver_.Bind(std::move(receiver));
  }
  ~MockBrowserStorage() override = default;

  MOCK_METHOD(void, Open, (const std::string&, OpenCallback));

  mojo::Receiver<BrowserStorage> receiver_{this};
};

class MockCdmFile : public media::mojom::CdmFile {
 public:
  MockCdmFile() = default;
  ~MockCdmFile() override = default;

  mojo::PendingAssociatedRemote<CdmFile> CreateRemote() {
    mojo::PendingAssociatedRemote<CdmFile> remote;
    receiver_.Bind(remote.InitWithNewEndpointAndPassReceiver());
    return remote;
  }

  MOCK_METHOD(void, Read, (ReadCallback));
  MOCK_METHOD(void, Write, (const std::vector<uint8_t>&, WriteCallback));

 private:
  mojo::AssociatedReceiver<CdmFile> receiver_{this};
};

class MockFrameInterfaceFactory : public media::mojom::FrameInterfaceFactory {
 public:
  MockFrameInterfaceFactory() = default;
  ~MockFrameInterfaceFactory() override = default;

  MOCK_METHOD(void,
              CreateProvisionFetcher,
              (mojo::PendingReceiver<media::mojom::ProvisionFetcher>));
  MOCK_METHOD(void, CreateCdmStorage, (mojo::PendingReceiver<BrowserStorage>));
  MOCK_METHOD(bool, GetCdmOrigin, (url::Origin*));
  MOCK_METHOD(void, GetCdmOrigin, (GetCdmOriginCallback));
  MOCK_METHOD(void, BindEmbedderReceiver, (mojo::GenericPendingReceiver));
};

}  // namespace

class CdmStorageAdapterTest : public testing::Test {
 protected:
  CdmStorageAdapterTest() {
    mock_frame_interface_factory_ =
        std::make_unique<MockFrameInterfaceFactory>();
    EXPECT_CALL(*mock_frame_interface_factory_, CreateCdmStorage(_))
        .WillOnce([this](mojo::PendingReceiver<BrowserStorage> receiver) {
          mock_browser_storage_ =
              std::make_unique<MockBrowserStorage>(std::move(receiver));
        });
    cdm_storage_adapter_ = std::make_unique<CdmStorageAdapter>(
        mock_frame_interface_factory_.get(),
        daemon_storage_mojo_.BindNewEndpointAndPassDedicatedReceiver());
    task_environment_.RunUntilIdle();
  }

  ~CdmStorageAdapterTest() override = default;

  std::unique_ptr<MockBrowserStorage> mock_browser_storage_;
  std::unique_ptr<CdmStorageAdapter> cdm_storage_adapter_;
  std::unique_ptr<MockFrameInterfaceFactory> mock_frame_interface_factory_;
  mojo::AssociatedRemote<DaemonStorage> daemon_storage_mojo_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CdmStorageAdapterTest, Read_Success) {
  std::vector<uint8_t> expected_data({1, 2, 3});
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Read(_))
      .WillOnce([expected_data](CdmFile::ReadCallback callback) {
        std::move(callback).Run(CdmFile::Status::kSuccess, expected_data);
      });
  base::MockCallback<DaemonStorage::ReadCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(true, expected_data));
  daemon_storage_mojo_->Read(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Read_OpenFailure) {
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([](const std::string& filename,
                   BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kFailure,
                                mojo::NullAssociatedRemote());
      });
  base::MockCallback<DaemonStorage::ReadCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false, _));
  daemon_storage_mojo_->Read(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Read_ReadFailure) {
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Read(_)).WillOnce([](CdmFile::ReadCallback callback) {
    std::move(callback).Run(CdmFile::Status::kFailure, {});
  });
  base::MockCallback<DaemonStorage::ReadCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false, _));
  daemon_storage_mojo_->Read(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Write_Success) {
  std::vector<uint8_t> expected_data({1, 2, 3});
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Write(expected_data, _))
      .WillOnce([expected_data](const std::vector<uint8_t>& data,
                                CdmFile::WriteCallback callback) {
        std::move(callback).Run(CdmFile::Status::kSuccess);
      });
  base::MockCallback<DaemonStorage::WriteCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(true));
  daemon_storage_mojo_->Write(kFakeFilename, expected_data,
                              callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Write_OpenFailure) {
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([](const std::string& filename,
                   BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kFailure,
                                mojo::NullAssociatedRemote());
      });
  base::MockCallback<DaemonStorage::WriteCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  daemon_storage_mojo_->Write(kFakeFilename, std::vector<uint8_t>({1}),
                              callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Write_WriteFailure) {
  std::vector<uint8_t> expected_data({1, 2, 3});
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Write(expected_data, _))
      .WillOnce([expected_data](const std::vector<uint8_t>& data,
                                CdmFile::WriteCallback callback) {
        std::move(callback).Run(CdmFile::Status::kFailure);
      });
  base::MockCallback<DaemonStorage::WriteCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  daemon_storage_mojo_->Write(kFakeFilename, expected_data,
                              callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Exists_Success) {
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Read(_)).WillOnce([](CdmFile::ReadCallback callback) {
    std::move(callback).Run(CdmFile::Status::kSuccess,
                            std::vector<uint8_t>({1}));
  });
  base::MockCallback<DaemonStorage::ExistsCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(true));
  daemon_storage_mojo_->Exists(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Exists_OpenFailure) {
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([](const std::string& filename,
                   BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kFailure,
                                mojo::NullAssociatedRemote());
      });
  base::MockCallback<DaemonStorage::ExistsCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  daemon_storage_mojo_->Exists(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Exists_ReadFailure) {
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Read(_)).WillOnce([](CdmFile::ReadCallback callback) {
    std::move(callback).Run(CdmFile::Status::kFailure, {});
  });
  base::MockCallback<DaemonStorage::ExistsCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  daemon_storage_mojo_->Exists(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Exists_EmptyFile) {
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Read(_)).WillOnce([](CdmFile::ReadCallback callback) {
    std::move(callback).Run(CdmFile::Status::kSuccess, {});
  });
  base::MockCallback<DaemonStorage::ExistsCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  daemon_storage_mojo_->Exists(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, GetSize_Success) {
  constexpr size_t kFakeSize = 42;
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Read(_)).WillOnce([](CdmFile::ReadCallback callback) {
    std::move(callback).Run(CdmFile::Status::kSuccess,
                            std::vector<uint8_t>(kFakeSize));
  });
  base::MockCallback<DaemonStorage::GetSizeCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(true, kFakeSize));
  daemon_storage_mojo_->GetSize(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, GetSize_OpenFailure) {
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([](const std::string& filename,
                   BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kFailure,
                                mojo::NullAssociatedRemote());
      });
  base::MockCallback<DaemonStorage::GetSizeCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false, _));
  daemon_storage_mojo_->GetSize(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, GetSize_ReadFailure) {
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Read(_)).WillOnce([](CdmFile::ReadCallback callback) {
    std::move(callback).Run(CdmFile::Status::kFailure, {});
  });
  base::MockCallback<DaemonStorage::GetSizeCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false, _));
  daemon_storage_mojo_->GetSize(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Remove_Success) {
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Write(std::vector<uint8_t>(), _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   CdmFile::WriteCallback callback) {
        std::move(callback).Run(CdmFile::Status::kSuccess);
      });
  base::MockCallback<DaemonStorage::RemoveCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(true));
  daemon_storage_mojo_->Remove(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Remove_OpenFailure) {
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([](const std::string& filename,
                   BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kFailure,
                                mojo::NullAssociatedRemote());
      });
  base::MockCallback<DaemonStorage::RemoveCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  daemon_storage_mojo_->Remove(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CdmStorageAdapterTest, Remove_WriteFailure) {
  MockCdmFile mock_file;
  EXPECT_CALL(*mock_browser_storage_, Open(kFakeFilename, _))
      .WillOnce([&mock_file](const std::string& filename,
                             BrowserStorage::OpenCallback callback) {
        std::move(callback).Run(BrowserStorage::Status::kSuccess,
                                mock_file.CreateRemote());
      });
  EXPECT_CALL(mock_file, Write(std::vector<uint8_t>(), _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   CdmFile::WriteCallback callback) {
        std::move(callback).Run(CdmFile::Status::kFailure);
      });
  base::MockCallback<DaemonStorage::RemoveCallback> callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  daemon_storage_mojo_->Remove(kFakeFilename, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
