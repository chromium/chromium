// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/common/semantic_tree.h"
#include <cstdint>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

constexpr size_t kInitialNumTopics = 349;
constexpr int kMaxTaxonomyVersion = 2;

constexpr Topic kNullTopic = Topic(0);

// kChildToParent stores the first parent for each topic. This data structure
// was chosen to reduce the binary size, since most topics have at most one
// parent. Additional parents are contained in kChildToAdditionalParents.
const uint16_t kChildToFirstParent[] = {
    0,   1,   1,   352, 1,   1,   1,   7,   352, 1,   1,   1,   12,  12,  12,
    12,  12,  12,  12,  12,  12,  12,  1,   23,  23,  23,  23,  23,  23,  23,
    23,  23,  23,  33,  33,  33,  23,  23,  23,  23,  40,  1,   1,   1,   363,
    45,  45,  45,  48,  45,  45,  45,  1,   53,  53,  53,  0,   57,  57,  57,
    57,  57,  62,  62,  62,  62,  62,  62,  62,  62,  62,  377, 62,  62,  62,
    377, 76,  377, 57,  57,  57,  57,  57,  83,  57,  0,   86,  86,  383, 385,
    88,  88,  385, 88,  388, 86,  86,  97,  86,  0,   100, 100, 0,   103, 104,
    103, 106, 103, 103, 103, 110, 110, 403, 103, 114, 103, 103, 117, 103, 103,
    103, 103, 103, 103, 103, 0,   126, 420, 126, 129, 129, 129, 129, 420, 420,
    126, 126, 137, 126, 126, 442, 140, 140, 442, 140, 140, 140, 140, 0,   149,
    150, 447, 149, 153, 149, 155, 447, 149, 158, 158, 158, 158, 158, 149, 164,
    164, 164, 164, 164, 447, 447, 0,   172, 173, 173, 175, 176, 173, 172, 0,
    180, 180, 180, 183, 183, 183, 183, 183, 183, 183, 183, 183, 180, 180, 180,
    0,   196, 196, 196, 196, 196, 201, 201, 196, 196, 196, 0,   519, 207, 207,
    207, 207, 207, 519, 0,   215, 530, 530, 215, 215, 215, 215, 215, 533, 215,
    0,   226, 227, 227, 227, 227, 231, 227, 227, 227, 226, 236, 236, 0,   239,
    239, 241, 0,   243, 243, 243, 243, 243, 243, 0,   250, 250, 250, 0,   254,
    255, 255, 255, 258, 258, 258, 254, 0,   263, 263, 265, 265, 265, 265, 265,
    263, 0,   272, 272, 0,   275, 275, 275, 0,   279, 279, 281, 279, 279, 279,
    279, 279, 279, 0,   289, 572, 289, 292, 572, 601, 572, 601, 572, 0,   299,
    299, 299, 299, 299, 299, 299, 299, 299, 299, 299, 299, 299, 312, 299, 299,
    299, 299, 299, 299, 299, 299, 299, 299, 621, 299, 299, 620, 299, 299, 299,
    299, 0,   332, 332, 332, 332, 332, 332, 332, 332, 332, 332, 332, 332, 344,
    344, 344, 344, 332, 1,   1,   1,   352, 352, 352, 352, 352, 12,  23,  33,
    1,   361, 1,   363, 363, 363, 363, 53,  57,  57,  57,  57,  62,  62,  67,
    62,  62,  81,  81,  86,  380, 86,  88,  383, 88,  385, 88,  88,  97,  389,
    97,  97,  97,  97,  99,  100, 100, 100, 100, 100, 100, 103, 103, 103, 404,
    404, 404, 404, 408, 404, 103, 103, 103, 103, 103, 415, 103, 103, 103, 126,
    420, 420, 126, 129, 424, 424, 424, 129, 129, 129, 129, 431, 129, 126, 434,
    126, 137, 137, 140, 439, 439, 140, 442, 149, 444, 444, 149, 447, 447, 172,
    450, 450, 450, 450, 450, 450, 450, 457, 450, 172, 172, 172, 462, 462, 180,
    183, 183, 180, 196, 196, 196, 201, 207, 473, 473, 475, 475, 475, 207, 210,
    210, 207, 482, 482, 482, 482, 482, 487, 482, 482, 211, 211, 211, 211, 211,
    211, 211, 207, 498, 207, 213, 213, 207, 503, 503, 503, 207, 507, 508, 508,
    507, 507, 507, 507, 507, 515, 515, 515, 207, 519, 519, 521, 207, 207, 215,
    525, 215, 215, 528, 215, 530, 215, 215, 533, 533, 533, 227, 227, 227, 227,
    227, 227, 227, 227, 227, 227, 226, 238, 238, 238, 238, 238, 238, 238, 238,
    238, 238, 238, 236, 239, 243, 254, 258, 272, 272, 565, 565, 567, 565, 272,
    570, 289, 572, 572, 572, 575, 572, 577, 578, 577, 577, 577, 572, 583, 572,
    585, 585, 585, 572, 572, 572, 572, 572, 572, 572, 572, 572, 572, 298, 298,
    289, 289, 289, 289, 604, 604, 289, 289, 299, 299, 299, 611, 611, 611, 611,
    611, 611, 611, 611, 299, 299, 340, 343, 332, 332, 332, 626, 626, 626};

static_assert(SemanticTree::kNumTopics == std::size(kChildToFirstParent));

// TaxonomyUpdate represents the incremental change from one taxonomy to the
// next. The structure assumes that a topic won't be added back after being
// deleted. It can be modified to support that use case by adding a field of
// IDs that are reintroduced.
struct TaxonomyUpdate {
  // The number of topics in the new taxonomy.
  const size_t taxonomy_size;
  // The maximum topic ID in the new taxonomy.
  const size_t max_topic_id;
  // A map from the new or renamed topic ids to their associated localized name
  // message ID.
  base::flat_map<uint16_t, uint16_t> renamed_topics;
  // The topics that have been deleted since the prior taxonomy version.
  const base::flat_set<uint16_t> deleted_topics;
};

TaxonomyUpdate* GetTaxonomyUpdateForTaxonomy2() {
  static base::NoDestructor<TaxonomyUpdate> taxonomy_update(
      {.taxonomy_size = 469,
       .max_topic_id = 629,
       .deleted_topics = {
           2,   3,   5,   6,   7,   8,   10,  11,  14,  16,  17,  22,  34,  35,
           37,  38,  39,  40,  41,  42,  43,  44,  45,  49,  54,  55,  58,  61,
           77,  79,  80,  85,  87,  98,  105, 106, 107, 108, 109, 110, 111, 112,
           114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 127, 130,
           133, 136, 138, 139, 142, 143, 145, 146, 147, 148, 155, 156, 165, 166,
           167, 168, 169, 174, 175, 178, 179, 181, 182, 188, 189, 190, 193, 195,
           197, 198, 199, 200, 203, 204, 205, 206, 216, 219, 220, 221, 222, 223,
           225, 228, 232, 235, 240, 241, 244, 246, 248, 251, 252, 255, 256, 257,
           261, 262, 266, 269, 270, 271, 273, 274, 275, 276, 278, 279, 280, 281,
           282, 283, 284, 285, 286, 287, 288, 290, 292, 302, 305, 306, 307, 308,
           311, 312, 313, 314, 316, 318, 319, 320, 326, 329, 330, 331, 333, 339,
           342, 344, 346, 347, 348, 349}});
  if (taxonomy_update->renamed_topics.empty()) {
    for (uint16_t i = 350; i <= 629; ++i) {
      taxonomy_update->renamed_topics[i] =
          IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V2_TOPIC_ID_350 + i - 350;
    }
  }
  return taxonomy_update.get();
}

// Each incremental taxonomy update after taxonomy 1.
const std::vector<TaxonomyUpdate*>& GetTaxonomyUpdates() {
  static const base::NoDestructor<std::vector<TaxonomyUpdate*>>
      kTaxonomyUpdates{{GetTaxonomyUpdateForTaxonomy2()}};
  return *kTaxonomyUpdates;
}

// Stores pre-computed results from GetTopicsInTaxonomy for each
// TaxonomyUpdate.
std::vector<std::vector<Topic>>& GetTopicsForEachTaxonomyUpdate() {
  static base::NoDestructor<std::vector<std::vector<Topic>>>
      topics_in_taxonomies(GetTaxonomyUpdates().size());
  return *topics_in_taxonomies;
}

static_assert(IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_349 -
                  IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_1 + 1 ==
              kInitialNumTopics);

// These asserts also have a side-effect of preventing unused resource
// removal from removing them.
#define ASSERT_RESOURCE_ID(num)                                              \
  static_assert(IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_##num -      \
                    IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_1 + 1 == \
                num)
ASSERT_RESOURCE_ID(2);
ASSERT_RESOURCE_ID(3);
ASSERT_RESOURCE_ID(4);
ASSERT_RESOURCE_ID(5);
ASSERT_RESOURCE_ID(6);
ASSERT_RESOURCE_ID(7);
ASSERT_RESOURCE_ID(8);
ASSERT_RESOURCE_ID(9);
ASSERT_RESOURCE_ID(10);
ASSERT_RESOURCE_ID(11);
ASSERT_RESOURCE_ID(12);
ASSERT_RESOURCE_ID(13);
ASSERT_RESOURCE_ID(14);
ASSERT_RESOURCE_ID(15);
ASSERT_RESOURCE_ID(16);
ASSERT_RESOURCE_ID(17);
ASSERT_RESOURCE_ID(18);
ASSERT_RESOURCE_ID(19);
ASSERT_RESOURCE_ID(20);
ASSERT_RESOURCE_ID(21);
ASSERT_RESOURCE_ID(22);
ASSERT_RESOURCE_ID(23);
ASSERT_RESOURCE_ID(24);
ASSERT_RESOURCE_ID(25);
ASSERT_RESOURCE_ID(26);
ASSERT_RESOURCE_ID(27);
ASSERT_RESOURCE_ID(28);
ASSERT_RESOURCE_ID(29);
ASSERT_RESOURCE_ID(30);
ASSERT_RESOURCE_ID(31);
ASSERT_RESOURCE_ID(32);
ASSERT_RESOURCE_ID(33);
ASSERT_RESOURCE_ID(34);
ASSERT_RESOURCE_ID(35);
ASSERT_RESOURCE_ID(36);
ASSERT_RESOURCE_ID(37);
ASSERT_RESOURCE_ID(38);
ASSERT_RESOURCE_ID(39);
ASSERT_RESOURCE_ID(40);
ASSERT_RESOURCE_ID(41);
ASSERT_RESOURCE_ID(42);
ASSERT_RESOURCE_ID(43);
ASSERT_RESOURCE_ID(44);
ASSERT_RESOURCE_ID(45);
ASSERT_RESOURCE_ID(46);
ASSERT_RESOURCE_ID(47);
ASSERT_RESOURCE_ID(48);
ASSERT_RESOURCE_ID(49);
ASSERT_RESOURCE_ID(50);
ASSERT_RESOURCE_ID(51);
ASSERT_RESOURCE_ID(52);
ASSERT_RESOURCE_ID(53);
ASSERT_RESOURCE_ID(54);
ASSERT_RESOURCE_ID(55);
ASSERT_RESOURCE_ID(56);
ASSERT_RESOURCE_ID(57);
ASSERT_RESOURCE_ID(58);
ASSERT_RESOURCE_ID(59);
ASSERT_RESOURCE_ID(60);
ASSERT_RESOURCE_ID(61);
ASSERT_RESOURCE_ID(62);
ASSERT_RESOURCE_ID(63);
ASSERT_RESOURCE_ID(64);
ASSERT_RESOURCE_ID(65);
ASSERT_RESOURCE_ID(66);
ASSERT_RESOURCE_ID(67);
ASSERT_RESOURCE_ID(68);
ASSERT_RESOURCE_ID(69);
ASSERT_RESOURCE_ID(70);
ASSERT_RESOURCE_ID(71);
ASSERT_RESOURCE_ID(72);
ASSERT_RESOURCE_ID(73);
ASSERT_RESOURCE_ID(74);
ASSERT_RESOURCE_ID(75);
ASSERT_RESOURCE_ID(76);
ASSERT_RESOURCE_ID(77);
ASSERT_RESOURCE_ID(78);
ASSERT_RESOURCE_ID(79);
ASSERT_RESOURCE_ID(80);
ASSERT_RESOURCE_ID(81);
ASSERT_RESOURCE_ID(82);
ASSERT_RESOURCE_ID(83);
ASSERT_RESOURCE_ID(84);
ASSERT_RESOURCE_ID(85);
ASSERT_RESOURCE_ID(86);
ASSERT_RESOURCE_ID(87);
ASSERT_RESOURCE_ID(88);
ASSERT_RESOURCE_ID(89);
ASSERT_RESOURCE_ID(90);
ASSERT_RESOURCE_ID(91);
ASSERT_RESOURCE_ID(92);
ASSERT_RESOURCE_ID(93);
ASSERT_RESOURCE_ID(94);
ASSERT_RESOURCE_ID(95);
ASSERT_RESOURCE_ID(96);
ASSERT_RESOURCE_ID(97);
ASSERT_RESOURCE_ID(98);
ASSERT_RESOURCE_ID(99);
ASSERT_RESOURCE_ID(100);
ASSERT_RESOURCE_ID(101);
ASSERT_RESOURCE_ID(102);
ASSERT_RESOURCE_ID(103);
ASSERT_RESOURCE_ID(104);
ASSERT_RESOURCE_ID(105);
ASSERT_RESOURCE_ID(106);
ASSERT_RESOURCE_ID(107);
ASSERT_RESOURCE_ID(108);
ASSERT_RESOURCE_ID(109);
ASSERT_RESOURCE_ID(110);
ASSERT_RESOURCE_ID(111);
ASSERT_RESOURCE_ID(112);
ASSERT_RESOURCE_ID(113);
ASSERT_RESOURCE_ID(114);
ASSERT_RESOURCE_ID(115);
ASSERT_RESOURCE_ID(116);
ASSERT_RESOURCE_ID(117);
ASSERT_RESOURCE_ID(118);
ASSERT_RESOURCE_ID(119);
ASSERT_RESOURCE_ID(120);
ASSERT_RESOURCE_ID(121);
ASSERT_RESOURCE_ID(122);
ASSERT_RESOURCE_ID(123);
ASSERT_RESOURCE_ID(124);
ASSERT_RESOURCE_ID(125);
ASSERT_RESOURCE_ID(126);
ASSERT_RESOURCE_ID(127);
ASSERT_RESOURCE_ID(128);
ASSERT_RESOURCE_ID(129);
ASSERT_RESOURCE_ID(130);
ASSERT_RESOURCE_ID(131);
ASSERT_RESOURCE_ID(132);
ASSERT_RESOURCE_ID(133);
ASSERT_RESOURCE_ID(134);
ASSERT_RESOURCE_ID(135);
ASSERT_RESOURCE_ID(136);
ASSERT_RESOURCE_ID(137);
ASSERT_RESOURCE_ID(138);
ASSERT_RESOURCE_ID(139);
ASSERT_RESOURCE_ID(140);
ASSERT_RESOURCE_ID(141);
ASSERT_RESOURCE_ID(142);
ASSERT_RESOURCE_ID(143);
ASSERT_RESOURCE_ID(144);
ASSERT_RESOURCE_ID(145);
ASSERT_RESOURCE_ID(146);
ASSERT_RESOURCE_ID(147);
ASSERT_RESOURCE_ID(148);
ASSERT_RESOURCE_ID(149);
ASSERT_RESOURCE_ID(150);
ASSERT_RESOURCE_ID(151);
ASSERT_RESOURCE_ID(152);
ASSERT_RESOURCE_ID(153);
ASSERT_RESOURCE_ID(154);
ASSERT_RESOURCE_ID(155);
ASSERT_RESOURCE_ID(156);
ASSERT_RESOURCE_ID(157);
ASSERT_RESOURCE_ID(158);
ASSERT_RESOURCE_ID(159);
ASSERT_RESOURCE_ID(160);
ASSERT_RESOURCE_ID(161);
ASSERT_RESOURCE_ID(162);
ASSERT_RESOURCE_ID(163);
ASSERT_RESOURCE_ID(164);
ASSERT_RESOURCE_ID(165);
ASSERT_RESOURCE_ID(166);
ASSERT_RESOURCE_ID(167);
ASSERT_RESOURCE_ID(168);
ASSERT_RESOURCE_ID(169);
ASSERT_RESOURCE_ID(170);
ASSERT_RESOURCE_ID(171);
ASSERT_RESOURCE_ID(172);
ASSERT_RESOURCE_ID(173);
ASSERT_RESOURCE_ID(174);
ASSERT_RESOURCE_ID(175);
ASSERT_RESOURCE_ID(176);
ASSERT_RESOURCE_ID(177);
ASSERT_RESOURCE_ID(178);
ASSERT_RESOURCE_ID(179);
ASSERT_RESOURCE_ID(180);
ASSERT_RESOURCE_ID(181);
ASSERT_RESOURCE_ID(182);
ASSERT_RESOURCE_ID(183);
ASSERT_RESOURCE_ID(184);
ASSERT_RESOURCE_ID(185);
ASSERT_RESOURCE_ID(186);
ASSERT_RESOURCE_ID(187);
ASSERT_RESOURCE_ID(188);
ASSERT_RESOURCE_ID(189);
ASSERT_RESOURCE_ID(190);
ASSERT_RESOURCE_ID(191);
ASSERT_RESOURCE_ID(192);
ASSERT_RESOURCE_ID(193);
ASSERT_RESOURCE_ID(194);
ASSERT_RESOURCE_ID(195);
ASSERT_RESOURCE_ID(196);
ASSERT_RESOURCE_ID(197);
ASSERT_RESOURCE_ID(198);
ASSERT_RESOURCE_ID(199);
ASSERT_RESOURCE_ID(200);
ASSERT_RESOURCE_ID(201);
ASSERT_RESOURCE_ID(202);
ASSERT_RESOURCE_ID(203);
ASSERT_RESOURCE_ID(204);
ASSERT_RESOURCE_ID(205);
ASSERT_RESOURCE_ID(206);
ASSERT_RESOURCE_ID(207);
ASSERT_RESOURCE_ID(208);
ASSERT_RESOURCE_ID(209);
ASSERT_RESOURCE_ID(210);
ASSERT_RESOURCE_ID(211);
ASSERT_RESOURCE_ID(212);
ASSERT_RESOURCE_ID(213);
ASSERT_RESOURCE_ID(214);
ASSERT_RESOURCE_ID(215);
ASSERT_RESOURCE_ID(216);
ASSERT_RESOURCE_ID(217);
ASSERT_RESOURCE_ID(218);
ASSERT_RESOURCE_ID(219);
ASSERT_RESOURCE_ID(220);
ASSERT_RESOURCE_ID(221);
ASSERT_RESOURCE_ID(222);
ASSERT_RESOURCE_ID(223);
ASSERT_RESOURCE_ID(224);
ASSERT_RESOURCE_ID(225);
ASSERT_RESOURCE_ID(226);
ASSERT_RESOURCE_ID(227);
ASSERT_RESOURCE_ID(228);
ASSERT_RESOURCE_ID(229);
ASSERT_RESOURCE_ID(230);
ASSERT_RESOURCE_ID(231);
ASSERT_RESOURCE_ID(232);
ASSERT_RESOURCE_ID(233);
ASSERT_RESOURCE_ID(234);
ASSERT_RESOURCE_ID(235);
ASSERT_RESOURCE_ID(236);
ASSERT_RESOURCE_ID(237);
ASSERT_RESOURCE_ID(238);
ASSERT_RESOURCE_ID(239);
ASSERT_RESOURCE_ID(240);
ASSERT_RESOURCE_ID(241);
ASSERT_RESOURCE_ID(242);
ASSERT_RESOURCE_ID(243);
ASSERT_RESOURCE_ID(244);
ASSERT_RESOURCE_ID(245);
ASSERT_RESOURCE_ID(246);
ASSERT_RESOURCE_ID(247);
ASSERT_RESOURCE_ID(248);
ASSERT_RESOURCE_ID(249);
ASSERT_RESOURCE_ID(250);
ASSERT_RESOURCE_ID(251);
ASSERT_RESOURCE_ID(252);
ASSERT_RESOURCE_ID(253);
ASSERT_RESOURCE_ID(254);
ASSERT_RESOURCE_ID(255);
ASSERT_RESOURCE_ID(256);
ASSERT_RESOURCE_ID(257);
ASSERT_RESOURCE_ID(258);
ASSERT_RESOURCE_ID(259);
ASSERT_RESOURCE_ID(260);
ASSERT_RESOURCE_ID(261);
ASSERT_RESOURCE_ID(262);
ASSERT_RESOURCE_ID(263);
ASSERT_RESOURCE_ID(264);
ASSERT_RESOURCE_ID(265);
ASSERT_RESOURCE_ID(266);
ASSERT_RESOURCE_ID(267);
ASSERT_RESOURCE_ID(268);
ASSERT_RESOURCE_ID(269);
ASSERT_RESOURCE_ID(270);
ASSERT_RESOURCE_ID(271);
ASSERT_RESOURCE_ID(272);
ASSERT_RESOURCE_ID(273);
ASSERT_RESOURCE_ID(274);
ASSERT_RESOURCE_ID(275);
ASSERT_RESOURCE_ID(276);
ASSERT_RESOURCE_ID(277);
ASSERT_RESOURCE_ID(278);
ASSERT_RESOURCE_ID(279);
ASSERT_RESOURCE_ID(280);
ASSERT_RESOURCE_ID(281);
ASSERT_RESOURCE_ID(282);
ASSERT_RESOURCE_ID(283);
ASSERT_RESOURCE_ID(284);
ASSERT_RESOURCE_ID(285);
ASSERT_RESOURCE_ID(286);
ASSERT_RESOURCE_ID(287);
ASSERT_RESOURCE_ID(288);
ASSERT_RESOURCE_ID(289);
ASSERT_RESOURCE_ID(290);
ASSERT_RESOURCE_ID(291);
ASSERT_RESOURCE_ID(292);
ASSERT_RESOURCE_ID(293);
ASSERT_RESOURCE_ID(294);
ASSERT_RESOURCE_ID(295);
ASSERT_RESOURCE_ID(296);
ASSERT_RESOURCE_ID(297);
ASSERT_RESOURCE_ID(298);
ASSERT_RESOURCE_ID(299);
ASSERT_RESOURCE_ID(300);
ASSERT_RESOURCE_ID(301);
ASSERT_RESOURCE_ID(302);
ASSERT_RESOURCE_ID(303);
ASSERT_RESOURCE_ID(304);
ASSERT_RESOURCE_ID(305);
ASSERT_RESOURCE_ID(306);
ASSERT_RESOURCE_ID(307);
ASSERT_RESOURCE_ID(308);
ASSERT_RESOURCE_ID(309);
ASSERT_RESOURCE_ID(310);
ASSERT_RESOURCE_ID(311);
ASSERT_RESOURCE_ID(312);
ASSERT_RESOURCE_ID(313);
ASSERT_RESOURCE_ID(314);
ASSERT_RESOURCE_ID(315);
ASSERT_RESOURCE_ID(316);
ASSERT_RESOURCE_ID(317);
ASSERT_RESOURCE_ID(318);
ASSERT_RESOURCE_ID(319);
ASSERT_RESOURCE_ID(320);
ASSERT_RESOURCE_ID(321);
ASSERT_RESOURCE_ID(322);
ASSERT_RESOURCE_ID(323);
ASSERT_RESOURCE_ID(324);
ASSERT_RESOURCE_ID(325);
ASSERT_RESOURCE_ID(326);
ASSERT_RESOURCE_ID(327);
ASSERT_RESOURCE_ID(328);
ASSERT_RESOURCE_ID(329);
ASSERT_RESOURCE_ID(330);
ASSERT_RESOURCE_ID(331);
ASSERT_RESOURCE_ID(332);
ASSERT_RESOURCE_ID(333);
ASSERT_RESOURCE_ID(334);
ASSERT_RESOURCE_ID(335);
ASSERT_RESOURCE_ID(336);
ASSERT_RESOURCE_ID(337);
ASSERT_RESOURCE_ID(338);
ASSERT_RESOURCE_ID(339);
ASSERT_RESOURCE_ID(340);
ASSERT_RESOURCE_ID(341);
ASSERT_RESOURCE_ID(342);
ASSERT_RESOURCE_ID(343);
ASSERT_RESOURCE_ID(344);
ASSERT_RESOURCE_ID(345);
ASSERT_RESOURCE_ID(346);
ASSERT_RESOURCE_ID(347);
ASSERT_RESOURCE_ID(348);
ASSERT_RESOURCE_ID(349);

bool IsTopicValid(Topic topic) {
  int i = static_cast<int>(topic);
  return i > 0 && i <= static_cast<int>(SemanticTree::kNumTopics);
}

std::vector<Topic> GetParentTopics(Topic topic) {
  if (kChildToFirstParent[static_cast<int>(topic) - 1] ==
      static_cast<int>(kNullTopic)) {
    return {};
  }
  std::vector<Topic> parents(
      {Topic(kChildToFirstParent[static_cast<int>(topic) - 1])});

  static const base::NoDestructor<
      base::flat_map<uint16_t, std::vector<uint16_t>>>
      kChildToAdditionalParents({{277, {227}}});
  auto it = kChildToAdditionalParents->find(topic.value());
  if (it != kChildToAdditionalParents->end()) {
    for (uint16_t parent_id : it->second) {
      parents.emplace_back(parent_id);
    }
  }
  return parents;
}

bool IsAncestorTopic(Topic src, Topic target) {
  std::vector<Topic> parent_topics = GetParentTopics(src);
  for (Topic topic : parent_topics) {
    if (topic == target || IsAncestorTopic(topic, target)) {
      return true;
    }
  }
  return false;
}

// Get the topics that are part of a taxonomy for taxonomy_version. Use
// this function only for taxonomy_version>1, since the first taxonomy is
// trivial (1-349).
const std::vector<Topic>& GetTopicsInTaxonomy(int taxonomy_version) {
  CHECK_GT(taxonomy_version, 1);
  CHECK_LE(taxonomy_version, kMaxTaxonomyVersion);
  if (GetTopicsForEachTaxonomyUpdate()[taxonomy_version - 2].empty()) {
    // Include topics up to the maximum topic id for the `taxonomy_version`,
    // and then remove the deleted topics.
    uint16_t max_topic_id =
        GetTaxonomyUpdates()[taxonomy_version - 2]->max_topic_id;
    base::flat_set<Topic> topics;
    for (uint16_t i = 1; i <= max_topic_id; ++i) {
      topics.emplace(i);
    }
    for (int taxonomy_version_i = 2; taxonomy_version_i <= taxonomy_version;
         ++taxonomy_version_i) {
      for (uint16_t i :
           GetTaxonomyUpdates()[taxonomy_version - 2]->deleted_topics) {
        topics.erase(Topic(i));
      }
    }
    CHECK_EQ(topics.size(),
             GetTaxonomyUpdates()[taxonomy_version - 2]->taxonomy_size);
    GetTopicsForEachTaxonomyUpdate()[taxonomy_version - 2] =
        std::vector(topics.begin(), topics.end());
  }
  return GetTopicsForEachTaxonomyUpdate()[taxonomy_version - 2];
}
}  // namespace

SemanticTree::SemanticTree() = default;
SemanticTree::~SemanticTree() = default;

Topic SemanticTree::GetRandomTopic(int taxonomy_version,
                                   uint64_t random_topic_index_decision) {
  CHECK(IsTaxonomySupported(taxonomy_version));
  if (taxonomy_version == 1) {
    size_t random_topic_index = random_topic_index_decision % kInitialNumTopics;
    return Topic(base::checked_cast<int>(random_topic_index + 1));
  }
  auto topics = GetTopicsInTaxonomy(taxonomy_version);
  size_t random_topic_index = random_topic_index_decision % topics.size();
  return topics[random_topic_index];
}

bool SemanticTree::IsTaxonomySupported(int taxonomy_version) {
  return taxonomy_version > 0 && taxonomy_version <= kMaxTaxonomyVersion;
}

std::vector<Topic> SemanticTree::GetDescendantTopics(const Topic& topic) {
  std::vector<Topic> ret;
  for (size_t i = 0; i < kNumTopics; ++i) {
    Topic cur_topic = Topic(i + 1);
    if (IsAncestorTopic(cur_topic, topic)) {
      ret.push_back(cur_topic);
    }
  }
  return ret;
}

std::vector<Topic> SemanticTree::GetAncestorTopics(const Topic& topic) {
  if (!IsTopicValid(topic)) {
    return {};
  }

  std::vector<Topic> ancestor_topics = GetParentTopics(topic);
  size_t unvisited_start_idx = 0;

  while (unvisited_start_idx < ancestor_topics.size()) {
    for (Topic parent : GetParentTopics(ancestor_topics[unvisited_start_idx])) {
      ancestor_topics.emplace_back(parent);
    }
    unvisited_start_idx++;
  }

  // Remove duplicate topics; duplicates can occur when a topic has two or more
  // parents which share part of a lineage.
  sort(ancestor_topics.begin(), ancestor_topics.end());
  ancestor_topics.erase(unique(ancestor_topics.begin(), ancestor_topics.end()),
                        ancestor_topics.end());
  return ancestor_topics;
}

absl::optional<int> SemanticTree::GetLatestLocalizedNameMessageId(
    const Topic& topic) {
  return SemanticTree::GetLocalizedNameMessageId(
      topic, blink::features::kBrowsingTopicsTaxonomyVersion.Get());
}

absl::optional<int> SemanticTree::GetLocalizedNameMessageId(
    const Topic& topic,
    int taxonomy_version) {
  if (!IsTopicValid(topic) || !IsTaxonomySupported(taxonomy_version)) {
    return absl::nullopt;
  }
  // Get the most recent name for a topic by iterating through the taxonomy
  // updates backwards.
  for (int taxonomy_version_i = taxonomy_version; taxonomy_version_i > 0;
       --taxonomy_version_i) {
    if (taxonomy_version_i == 1) {
      return IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_1 +
             static_cast<int>(topic) - 1;
    }
    auto renamed_topics_iterator =
        GetTaxonomyUpdates()[taxonomy_version_i - 2]->renamed_topics.find(
            static_cast<int>(topic));
    if (renamed_topics_iterator !=
        GetTaxonomyUpdates()[taxonomy_version_i - 2]->renamed_topics.end()) {
      return renamed_topics_iterator->second;
    }
  }
  return absl::nullopt;
}

}  // namespace browsing_topics
