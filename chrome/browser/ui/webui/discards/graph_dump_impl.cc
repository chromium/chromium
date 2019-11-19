// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/graph_dump_impl.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

int64_t GetSerializationId(const performance_manager::Node* node) {
  return performance_manager::Node::GetSerializationId(node);
}

}  // namespace

class DiscardsGraphDumpImpl::FaviconRequestHelper {
 public:
  FaviconRequestHelper(base::WeakPtr<DiscardsGraphDumpImpl> graph_dump,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);

  void RequestFavicon(GURL page_url,
                      performance_manager::WebContentsProxy contents_proxy,
                      int64_t serialization_id);
  void FaviconDataAvailable(int64_t serialization_id,
                            const favicon_base::FaviconRawBitmapResult& result);

 private:
  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  base::WeakPtr<DiscardsGraphDumpImpl> graph_dump_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FaviconRequestHelper);
};

DiscardsGraphDumpImpl::FaviconRequestHelper::FaviconRequestHelper(
    base::WeakPtr<DiscardsGraphDumpImpl> graph_dump,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : graph_dump_(graph_dump), task_runner_(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void DiscardsGraphDumpImpl::FaviconRequestHelper::RequestFavicon(
    GURL page_url,
    performance_manager::WebContentsProxy contents_proxy,
    int64_t serialization_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::WebContents* web_contents = contents_proxy.Get();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  if (!cancelable_task_tracker_)
    cancelable_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();

  constexpr size_t kIconSize = 16;
  constexpr bool kFallbackToHost = true;
  // It's safe to pass this unretained here, as the tasks are cancelled
  // on deletion of the cancelable task tracker.
  favicon_service->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon}, kIconSize, kFallbackToHost,
      base::BindRepeating(&FaviconRequestHelper::FaviconDataAvailable,
                          base::Unretained(this), serialization_id),
      cancelable_task_tracker_.get());
}

void DiscardsGraphDumpImpl::FaviconRequestHelper::FaviconDataAvailable(
    int64_t serialization_id,
    const favicon_base::FaviconRawBitmapResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.is_valid())
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DiscardsGraphDumpImpl::SendFaviconNotification,
                     graph_dump_, serialization_id, result.bitmap_data));
}

DiscardsGraphDumpImpl::DiscardsGraphDumpImpl() {}

DiscardsGraphDumpImpl::~DiscardsGraphDumpImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!graph_);
  DCHECK(!change_subscriber_);
  DCHECK(!favicon_request_helper_);
}

// static
void DiscardsGraphDumpImpl::CreateAndBind(
    mojo::PendingReceiver<discards::mojom::GraphDump> receiver,
    performance_manager::Graph* graph) {
  std::unique_ptr<DiscardsGraphDumpImpl> dump =
      std::make_unique<DiscardsGraphDumpImpl>();

  dump->BindWithGraph(graph, std::move(receiver));
  graph->PassToGraph(std::move(dump));
}

void DiscardsGraphDumpImpl::BindWithGraph(
    performance_manager::Graph* graph,
    mojo::PendingReceiver<discards::mojom::GraphDump> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &DiscardsGraphDumpImpl::OnConnectionError, base::Unretained(this)));
}

namespace {

template <typename FunctionType>
void ForFrameAndOffspring(const performance_manager::FrameNode* parent_frame,
                          FunctionType on_frame) {
  on_frame(parent_frame);

  for (const performance_manager::FrameNode* child_frame :
       parent_frame->GetChildFrameNodes())
    ForFrameAndOffspring(child_frame, on_frame);
}

}  // namespace

void DiscardsGraphDumpImpl::SubscribeToChanges(
    mojo::PendingRemote<discards::mojom::GraphChangeStream> change_subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_.Bind(std::move(change_subscriber));

  // Send creation notifications for all existing nodes.
  for (const performance_manager::ProcessNode* process_node :
       graph_->GetAllProcessNodes())
    SendProcessNotification(process_node, true);

  for (const performance_manager::PageNode* page_node :
       graph_->GetAllPageNodes()) {
    SendPageNotification(page_node, true);
    StartPageFaviconRequest(page_node);

    // Dispatch preorder frame notifications.
    for (const performance_manager::FrameNode* main_frame_node :
         page_node->GetMainFrameNodes()) {
      ForFrameAndOffspring(
          main_frame_node,
          [this](const performance_manager::FrameNode* frame_node) {
            this->SendFrameNotification(frame_node, true);
            this->StartFrameFaviconRequest(frame_node);
          });
    }
  }

  // Subscribe to subsequent notifications.
  graph_->AddFrameNodeObserver(this);
  graph_->AddPageNodeObserver(this);
  graph_->AddProcessNodeObserver(this);
}

void DiscardsGraphDumpImpl::OnPassedToGraph(performance_manager::Graph* graph) {
  DCHECK(!graph_);
  graph_ = graph;
}

void DiscardsGraphDumpImpl::OnTakenFromGraph(
    performance_manager::Graph* graph) {
  DCHECK_EQ(graph_, graph);

  if (change_subscriber_) {
    graph_->RemoveFrameNodeObserver(this);
    graph_->RemovePageNodeObserver(this);
    graph_->RemoveProcessNodeObserver(this);
  }

  change_subscriber_.reset();

  // The favicon helper must be deleted on the UI thread.
  if (favicon_request_helper_) {
    base::DeleteSoon(FROM_HERE, {content::BrowserThread::UI},
                     std::move(favicon_request_helper_));
  }

  graph_ = nullptr;
}

void DiscardsGraphDumpImpl::OnFrameNodeAdded(
    const performance_manager::FrameNode* frame_node) {
  SendFrameNotification(frame_node, true);
  StartFrameFaviconRequest(frame_node);
}

void DiscardsGraphDumpImpl::OnBeforeFrameNodeRemoved(
    const performance_manager::FrameNode* frame_node) {
  SendDeletionNotification(frame_node);
}

void DiscardsGraphDumpImpl::OnURLChanged(
    const performance_manager::FrameNode* frame_node,
    const GURL& previous_value) {
  SendFrameNotification(frame_node, false);
  StartFrameFaviconRequest(frame_node);
}

void DiscardsGraphDumpImpl::OnPageNodeAdded(
    const performance_manager::PageNode* page_node) {
  SendPageNotification(page_node, true);
  StartPageFaviconRequest(page_node);
}

void DiscardsGraphDumpImpl::OnBeforePageNodeRemoved(
    const performance_manager::PageNode* page_node) {
  SendDeletionNotification(page_node);
}

void DiscardsGraphDumpImpl::OnFaviconUpdated(
    const performance_manager::PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartPageFaviconRequest(page_node);
}

void DiscardsGraphDumpImpl::OnMainFrameUrlChanged(
    const performance_manager::PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendPageNotification(page_node, false);
}

void DiscardsGraphDumpImpl::OnProcessNodeAdded(
    const performance_manager::ProcessNode* process_node) {
  SendProcessNotification(process_node, true);
}

void DiscardsGraphDumpImpl::OnProcessLifetimeChange(
    const performance_manager::ProcessNode* process_node) {
  SendProcessNotification(process_node, false);
}

void DiscardsGraphDumpImpl::OnBeforeProcessNodeRemoved(
    const performance_manager::ProcessNode* process_node) {
  SendDeletionNotification(process_node);
}

DiscardsGraphDumpImpl::FaviconRequestHelper*
DiscardsGraphDumpImpl::EnsureFaviconRequestHelper() {
  if (!favicon_request_helper_) {
    favicon_request_helper_ = std::make_unique<FaviconRequestHelper>(
        weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get());
  }

  return favicon_request_helper_.get();
}

void DiscardsGraphDumpImpl::StartPageFaviconRequest(
    const performance_manager::PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!page_node->GetMainFrameUrl().is_valid())
    return;

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&FaviconRequestHelper::RequestFavicon,
                                base::Unretained(EnsureFaviconRequestHelper()),
                                page_node->GetMainFrameUrl(),
                                page_node->GetContentsProxy(),
                                GetSerializationId(page_node)));
}

void DiscardsGraphDumpImpl::StartFrameFaviconRequest(
    const performance_manager::FrameNode* frame_node) {
  if (!frame_node->GetURL().is_valid())
    return;

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&FaviconRequestHelper::RequestFavicon,
                                base::Unretained(EnsureFaviconRequestHelper()),
                                frame_node->GetURL(),
                                frame_node->GetPageNode()->GetContentsProxy(),
                                GetSerializationId(frame_node)));
}

void DiscardsGraphDumpImpl::SendFrameNotification(
    const performance_manager::FrameNode* frame,
    bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/961785): Add more frame properties.
  discards::mojom::FrameInfoPtr frame_info = discards::mojom::FrameInfo::New();

  frame_info->id = GetSerializationId(frame);

  auto* parent_frame = frame->GetParentFrameNode();
  frame_info->parent_frame_id = GetSerializationId(parent_frame);

  auto* process = frame->GetProcessNode();
  frame_info->process_id = GetSerializationId(process);

  auto* page = frame->GetPageNode();
  frame_info->page_id = GetSerializationId(page);

  frame_info->url = frame->GetURL();

  if (created)
    change_subscriber_->FrameCreated(std::move(frame_info));
  else
    change_subscriber_->FrameChanged(std::move(frame_info));
}

void DiscardsGraphDumpImpl::SendPageNotification(
    const performance_manager::PageNode* page_node,
    bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/961785): Add more page_node properties.
  discards::mojom::PageInfoPtr page_info = discards::mojom::PageInfo::New();

  page_info->id = GetSerializationId(page_node);
  page_info->main_frame_url = page_node->GetMainFrameUrl();
  if (created)
    change_subscriber_->PageCreated(std::move(page_info));
  else
    change_subscriber_->PageChanged(std::move(page_info));
}

void DiscardsGraphDumpImpl::SendProcessNotification(
    const performance_manager::ProcessNode* process,
    bool created) {
  // TODO(https://crbug.com/961785): Add more process properties.
  discards::mojom::ProcessInfoPtr process_info =
      discards::mojom::ProcessInfo::New();

  process_info->id = GetSerializationId(process);
  process_info->pid = process->GetProcessId();
  process_info->cumulative_cpu_usage = process->GetCumulativeCpuUsage();
  process_info->private_footprint_kb = process->GetPrivateFootprintKb();

  if (created)
    change_subscriber_->ProcessCreated(std::move(process_info));
  else
    change_subscriber_->ProcessChanged(std::move(process_info));
}

void DiscardsGraphDumpImpl::SendDeletionNotification(
    const performance_manager::Node* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_subscriber_->NodeDeleted(GetSerializationId(node));
}

void DiscardsGraphDumpImpl::SendFaviconNotification(
    int64_t serialization_id,
    scoped_refptr<base::RefCountedMemory> bitmap_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(0u, bitmap_data->size());

  discards::mojom::FavIconInfoPtr icon_info =
      discards::mojom::FavIconInfo::New();
  icon_info->node_id = serialization_id;

  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(bitmap_data->front()),
                        bitmap_data->size()),
      &icon_info->icon_data);

  change_subscriber_->FavIconDataAvailable(std::move(icon_info));
}

// static
void DiscardsGraphDumpImpl::OnConnectionError(DiscardsGraphDumpImpl* impl) {
  std::unique_ptr<GraphOwned> owned_impl = impl->graph_->TakeFromGraph(impl);
}
