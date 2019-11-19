// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_FACTORY_DOWNLOAD_SERVICE_FACTORY_HELPER_H_
#define COMPONENTS_DOWNLOAD_CONTENT_FACTORY_DOWNLOAD_SERVICE_FACTORY_HELPER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/download/public/background_service/blob_context_getter_factory.h"
#include "components/download/public/background_service/clients.h"

class SimpleFactoryKey;

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace download {

class DownloadService;
class SimpleDownloadManagerCoordinator;
class TaskScheduler;

// |clients| is a map of DownloadClient -> std::unique_ptr<Client>.  This
// represents all of the clients that are allowed to have requests made on
// their behalf.  This cannot be changed after startup.  Any existing requests
// no longer associated with a client will be cancelled.
// |storage_dir| is a path to where all the local storage will be.  This will
// hold the internal database as well as any temporary files on disk.  If this
// is an empty path, the service will not persist any information to disk and
// will act as an in-memory only service (this means no auto-retries after
// restarts, no files written on completion, etc.).
// |background_task_runner| will be used for all disk reads and writes.
std::unique_ptr<DownloadService> BuildDownloadService(
    SimpleFactoryKey* simple_factory_key,
    std::unique_ptr<DownloadClientMap> clients,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::FilePath& storage_dir,
    SimpleDownloadManagerCoordinator* download_manager_coordinator,
    leveldb_proto::ProtoDatabaseProvider* proto_db_provider,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    std::unique_ptr<TaskScheduler> task_scheduler);

// Create download service used in incognito mode, without any database or
// download file IO.
std::unique_ptr<DownloadService> BuildInMemoryDownloadService(
    SimpleFactoryKey* simple_factory_key,
    std::unique_ptr<DownloadClientMap> clients,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::FilePath& storage_dir,
    BlobContextGetterFactoryPtr blob_context_getter_factory,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_FACTORY_DOWNLOAD_SERVICE_FACTORY_HELPER_H_
