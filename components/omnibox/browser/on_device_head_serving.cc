// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_serving.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace {
// The offset of the root node for the tree. The first two bytes is reserved to
// specify the size (num of bytes) of the address and the score in each node.
const int kRootNodeOffset = 2;

uint32_t ConvertByteArrayToInt(char byte_array[], uint32_t num_bytes) {
  uint32_t result = 0;
  for (uint32_t i = 0; i < num_bytes; ++i) {
    result |= (byte_array[i] & 0xff) << (8 * i);
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<OnDeviceHeadServing> OnDeviceHeadServing::Create(
    const std::string& model_filename,
    int max_num_matches_to_return) {
  std::unique_ptr<OnDeviceHeadServing> serving = base::WrapUnique(
      new OnDeviceHeadServing(model_filename, max_num_matches_to_return));

  // TODO(crbug.com/925072): Add DCHECK and code to report failures to UMA
  // histogram.
  if (!serving->OpenModelFileStream(0)) {
    DVLOG(1) << "On Device Head Serving: cannot create on device head "
             << "serving instance because model file cannot be opened";
    return nullptr;
  }

  char sizes[2];
  if (!serving->ReadNextNumBytes(2, sizes)) {
    DVLOG(1) << "On Device Head Serving: failed to read size information "
             << "in the first 2 bytes of the model file: " << model_filename;
    serving->MaybeCloseModelFileStream();
    return nullptr;
  }
  serving->MaybeCloseModelFileStream();

  serving->address_size_ = sizes[0];
  serving->score_size_ = sizes[1];
  if (!serving->AreSizesValid()) {
    return nullptr;
  }

  return serving;
}

OnDeviceHeadServing::OnDeviceHeadServing(const std::string& model_filename,
                                         uint32_t max_num_matches_to_return)
    : model_filename_(model_filename),
      max_num_matches_to_return_(max_num_matches_to_return) {}

OnDeviceHeadServing::~OnDeviceHeadServing() {
  MaybeCloseModelFileStream();
}

bool OnDeviceHeadServing::AreSizesValid() {
  bool is_score_size_valid = (score_size_ >= 2 && score_size_ <= 4);
  bool is_address_size_valid = (address_size_ >= 3 && address_size_ <= 4);
  if (!is_score_size_valid) {
    DVLOG(1) << "On Device Head Serving: score size [" << score_size_
             << "] is not valid; valid size should 2, 3 or 4 bytes.";
  }
  if (!is_address_size_valid) {
    DVLOG(1) << "On Device Head Serving: address size [" << address_size_
             << "] is not valid; valid size should be 3 or 4 bytes.";
  }
  return is_score_size_valid && is_address_size_valid;
}

std::vector<std::pair<std::string, uint32_t>>
OnDeviceHeadServing::GetSuggestionsForPrefix(const std::string& prefix) {
  std::vector<std::pair<std::string, uint32_t>> suggestions;
  if (prefix.empty()) {
    return suggestions;
  }

  OpenModelFileStream(kRootNodeOffset);
  MatchCandidate start_match;
  if (FindStartNode(prefix, &start_match)) {
    suggestions = DoSearch(start_match);
  }
  MaybeCloseModelFileStream();
  return suggestions;
}

std::vector<std::pair<std::string, uint32_t>> OnDeviceHeadServing::DoSearch(
    const MatchCandidate& start_match) {
  std::vector<std::pair<std::string, uint32_t>> suggestions;

  CandidateQueue leaf_queue, non_leaf_queue;
  uint32_t min_score_in_queues = start_match.score;
  InsertCandidateToQueue(start_match, &leaf_queue, &non_leaf_queue);

  // Do the search until there is no non leaf candidates in the queue.
  while (!non_leaf_queue.empty()) {
    // Always fetch the intermediate node with highest score at the back of the
    // queue.
    auto next_candidates = ReadTreeNode(non_leaf_queue.back());
    non_leaf_queue.pop_back();
    min_score_in_queues = GetMinScoreFromQueues(leaf_queue, non_leaf_queue);

    for (const auto& candidate : next_candidates) {
      if (candidate.score > min_score_in_queues ||
          (leaf_queue.size() + non_leaf_queue.size() <
           max_num_matches_to_return_)) {
        InsertCandidateToQueue(candidate, &leaf_queue, &non_leaf_queue);
      }

      // If there are too many candidates in the queues, remove the one with
      // lowest score since it will never be shown to users.
      if (leaf_queue.size() + non_leaf_queue.size() >
          max_num_matches_to_return_) {
        if (leaf_queue.empty() ||
            (!non_leaf_queue.empty() &&
             leaf_queue.front().score > non_leaf_queue.front().score)) {
          non_leaf_queue.pop_front();
        } else {
          leaf_queue.pop_front();
        }
      }
      min_score_in_queues = GetMinScoreFromQueues(leaf_queue, non_leaf_queue);
    }
  }

  while (!leaf_queue.empty()) {
    suggestions.push_back(
        std::make_pair(leaf_queue.back().text, leaf_queue.back().score));
    leaf_queue.pop_back();
  }

  return suggestions;
}

void OnDeviceHeadServing::InsertCandidateToQueue(
    const MatchCandidate& candidate,
    CandidateQueue* leaf_queue,
    CandidateQueue* non_leaf_queue) {
  CandidateQueue* queue_ptr =
      candidate.is_complete_suggestion ? leaf_queue : non_leaf_queue;

  if (queue_ptr->empty() || candidate.score > queue_ptr->back().score) {
    queue_ptr->push_back(candidate);
  } else {
    auto iter = queue_ptr->begin();
    for (; iter != queue_ptr->end() && candidate.score > iter->score; ++iter) {
    }
    queue_ptr->insert(iter, candidate);
  }
}

uint32_t OnDeviceHeadServing::GetMinScoreFromQueues(
    const CandidateQueue& queue_1,
    const CandidateQueue& queue_2) {
  uint32_t min_score = 0x1 << (score_size_ * 8 - 1);
  if (!queue_1.empty()) {
    min_score = std::min(min_score, queue_1.front().score);
  }
  if (!queue_2.empty()) {
    min_score = std::min(min_score, queue_2.front().score);
  }
  return min_score;
}

bool OnDeviceHeadServing::FindStartNode(const std::string& prefix,
                                        MatchCandidate* start_match) {
  if (start_match == nullptr) {
    return false;
  }

  start_match->text = "";
  start_match->score = 0;
  start_match->address = kRootNodeOffset;
  start_match->is_complete_suggestion = false;

  while (start_match->text.size() < prefix.size()) {
    auto children = ReadTreeNode(*start_match);
    bool has_match = false;
    for (auto const& child : children) {
      // The way we build the model ensures that there will be only one child
      // matching the given prefix at each node.
      if (!child.text.empty() &&
          (base::StartsWith(child.text, prefix, base::CompareCase::SENSITIVE) ||
           base::StartsWith(prefix, child.text,
                            base::CompareCase::SENSITIVE))) {
        // A leaf only partially matching the given prefix cannot be the right
        // start node.
        if (child.is_complete_suggestion && child.text.size() < prefix.size()) {
          continue;
        }
        start_match->text = child.text;
        start_match->is_complete_suggestion = child.is_complete_suggestion;
        start_match->score = child.score;
        start_match->address = child.address;
        has_match = true;
        break;
      }
    }
    if (!has_match) {
      return false;
    }
  }

  return start_match->text.size() >= prefix.size();
}

uint32_t OnDeviceHeadServing::ReadMaxScoreAsRoot(uint32_t address,
                                                 MatchCandidate* leaf_candidate,
                                                 bool* is_successful) {
  if (is_successful == nullptr) {
    DVLOG(1) << "On Device Head Serving: a boolean var is_successful "
             << "is required when calling function ReadMaxScoreAsRoot";
    return 0;
  }

  model_filestream_.seekg(address);
  uint32_t max_score_block = ReadNextNumBytesAsInt(score_size_, is_successful);
  if (!*is_successful) {
    return 0;
  }

  // The 1st bit is the indicator so removing it when rebuilding the max
  // score as root.
  uint32_t max_score = max_score_block >> 1;

  // Read the leaf_score and set leaf_candidate when the indicator is 1.
  if ((max_score_block & 0x1) == 0x1 && leaf_candidate != nullptr) {
    uint32_t leaf_score = ReadNextNumBytesAsInt(score_size_, is_successful);
    if (!*is_successful) {
      return 0;
    }
    leaf_candidate->score = leaf_score;
    leaf_candidate->is_complete_suggestion = true;
  }
  return max_score;
}

bool OnDeviceHeadServing::ReadNextChild(MatchCandidate* candidate) {
  if (candidate == nullptr) {
    return false;
  }

  // Read block [length of text];
  bool is_successful;
  uint32_t text_length = ReadNextNumBytesAsInt(1, &is_successful);
  if (!is_successful) {
    return false;
  }

  // This is the end of the node.
  if (text_length == 0) {
    return false;
  }

  // Read block [text].
  char* text_buf = new char[text_length];
  if (!ReadNextNumBytes(text_length, text_buf)) {
    delete[] text_buf;
    return false;
  }
  std::string text(text_buf, text_length);
  delete[] text_buf;
  // Append the text in this child such that the MatchCandidate object always
  // contains the string representing the path from the root node to here.
  candidate->text = base::StrCat({candidate->text, text});

  // Read block [1 bit indicator + address/leaf_score]
  // First read the 1 bit indicator.
  char first_byte;
  if (!ReadNextNumBytes(1, &first_byte)) {
    return false;
  }
  bool is_leaf_score = (first_byte & 0x1) == 0x0;

  uint32_t length_of_leftover =
      (is_leaf_score ? score_size_ : address_size_) - 1;

  char* leftover = new char[length_of_leftover];
  is_successful = ReadNextNumBytes(length_of_leftover, leftover);

  if (is_successful) {
    char* last_block = new char[length_of_leftover + 1];
    std::memcpy(last_block, &first_byte, 1);
    std::memcpy(last_block + 1, leftover, length_of_leftover);
    // Remove the 1 bit indicator when re-constructing the score/address.
    uint32_t score_or_address =
        ConvertByteArrayToInt(last_block, length_of_leftover + 1) >> 1;

    if (is_leaf_score) {
      // Address is not required for leaf child.
      candidate->score = score_or_address;
      candidate->is_complete_suggestion = true;
    } else {
      // For non leaf child, score has been set as the max_score_as_root
      // found at the beginning of the current node.
      candidate->address = score_or_address;
      candidate->is_complete_suggestion = false;
    }
    delete[] last_block;
  }

  delete[] leftover;
  return is_successful;
}

std::vector<OnDeviceHeadServing::MatchCandidate>
OnDeviceHeadServing::ReadTreeNode(const MatchCandidate& current) {
  std::vector<MatchCandidate> candidates;
  // The current candidate passed in is a leaf node and we shall stop here.
  if (current.is_complete_suggestion) {
    return candidates;
  }

  bool is_successful;
  MatchCandidate leaf_candidate;
  leaf_candidate.is_complete_suggestion = false;

  uint32_t max_score_as_root =
      ReadMaxScoreAsRoot(current.address, &leaf_candidate, &is_successful);
  if (!is_successful) {
    DVLOG(1) << "On Device Head Serving: read max_score_as_root failed at "
             << "address [" << current.address << "]";
    return candidates;
  }

  // The max_score_as_root block may contain a leaf node which corresponds to a
  // valid suggestion. Its score was set in function ReadMaxScoreAsRoot.
  if (leaf_candidate.is_complete_suggestion) {
    leaf_candidate.text = current.text;
    candidates.push_back(leaf_candidate);
  }

  // Read child blocks until we reach the end of the node.
  while (true) {
    MatchCandidate candidate;
    candidate.text = current.text;
    candidate.score = max_score_as_root;
    if (!ReadNextChild(&candidate)) {
      break;
    }
    candidates.push_back(candidate);
  }
  return candidates;
}

bool OnDeviceHeadServing::ReadNextNumBytes(uint32_t num_bytes, char* buf) {
  uint32_t address = model_filestream_.tellg();
  model_filestream_.read(buf, num_bytes);
  if (model_filestream_.fail()) {
    DVLOG(1) << "On Device Head Serving: ifstream read error at address ["
             << address << "], when trying to read [" << num_bytes << "] bytes";
    return false;
  }
  return true;
}

uint32_t OnDeviceHeadServing::ReadNextNumBytesAsInt(uint32_t num_bytes,
                                                    bool* is_successful) {
  char* buf = new char[num_bytes];
  *is_successful = ReadNextNumBytes(num_bytes, buf);
  if (!*is_successful) {
    delete[] buf;
    return 0;
  }

  uint32_t result = ConvertByteArrayToInt(buf, num_bytes);
  delete[] buf;

  return result;
}

bool OnDeviceHeadServing::OpenModelFileStream(const uint32_t start_address) {
  // First close the file if it's still open.
  if (model_filestream_.is_open()) {
    LOG(WARNING) << "Previous file is still open";
    model_filestream_.close();
  }

  model_filestream_.open(model_filename_, std::ios::in | std::ios::binary);
  if (!model_filestream_.is_open()) {
    DVLOG(1) << "Failed to open model file from [" << model_filename_ << "]";
    return false;
  }

  if (start_address > 0) {
    model_filestream_.seekg(start_address);
  }
  return true;
}

void OnDeviceHeadServing::MaybeCloseModelFileStream() {
  if (model_filestream_.is_open()) {
    model_filestream_.close();
  }
}
