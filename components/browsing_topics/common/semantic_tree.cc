// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/common/semantic_tree.h"

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "components/strings/grit/components_strings.h"

namespace browsing_topics {

namespace {
// Stores a taxonomy version and a localized name message id for a topic

struct SemanticTreeTaxonomyInformation {
  SemanticTreeTaxonomyInformation(int taxonomy_version,
                                  int localized_name_message_id)
      : taxonomy_version(taxonomy_version),
        localized_name_message_id(localized_name_message_id) {}
  SemanticTreeTaxonomyInformation(
      const SemanticTreeTaxonomyInformation& other) = default;
  ~SemanticTreeTaxonomyInformation() = default;
  const int taxonomy_version;
  const int localized_name_message_id;
};

// Stores information for a topic's node in the semantic tree:
// the topic's parent and taxonomy information (version(s) and name(s))
struct SemanticTreeNodeInformation {
  SemanticTreeNodeInformation(
      const absl::optional<Topic>& parent_topic,
      const std::vector<SemanticTreeTaxonomyInformation>& taxonomy_information)
      : parent_topic(parent_topic),
        taxonomy_information(taxonomy_information) {}
  SemanticTreeNodeInformation(const SemanticTreeNodeInformation& other) =
      default;
  ~SemanticTreeNodeInformation() = default;
  const absl::optional<Topic> parent_topic;
  const std::vector<SemanticTreeTaxonomyInformation> taxonomy_information;
};

constexpr size_t kSemanticTreeSize = 349;

// Get the mapping of topic to information such as its parent topic,
// valid taxonomy, and message id for its localized name string
const base::
    fixed_flat_map<Topic, SemanticTreeNodeInformation, kSemanticTreeSize>&
    GetSemanticTreeInternal() {
  static const base::NoDestructor<base::fixed_flat_map<
      Topic, SemanticTreeNodeInformation, kSemanticTreeSize>>
      kSemanticTreeMap(base::MakeFixedFlatMapSorted<
                       Topic, SemanticTreeNodeInformation>(
          {{Topic(1),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_1)})},
           {Topic(2),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_2)})},
           {Topic(3),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_3)})},
           {Topic(4),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_4)})},
           {Topic(5),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_5)})},
           {Topic(6),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_6)})},
           {Topic(7),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_7)})},
           {Topic(8),
            SemanticTreeNodeInformation(
                Topic(7),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_8)})},
           {Topic(9),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_9)})},
           {Topic(10),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_10)})},
           {Topic(11),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_11)})},
           {Topic(12),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_12)})},
           {Topic(13),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_13)})},
           {Topic(14),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_14)})},
           {Topic(15),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_15)})},
           {Topic(16),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_16)})},
           {Topic(17),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_17)})},
           {Topic(18),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_18)})},
           {Topic(19),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_19)})},
           {Topic(20),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_20)})},
           {Topic(21),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_21)})},
           {Topic(22),
            SemanticTreeNodeInformation(
                Topic(12),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_22)})},
           {Topic(23),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_23)})},
           {Topic(24),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_24)})},
           {Topic(25),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_25)})},
           {Topic(26),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_26)})},
           {Topic(27),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_27)})},
           {Topic(28),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_28)})},
           {Topic(29),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_29)})},
           {Topic(30),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_30)})},
           {Topic(31),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_31)})},
           {Topic(32),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_32)})},
           {Topic(33),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_33)})},
           {Topic(34),
            SemanticTreeNodeInformation(
                Topic(33),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_34)})},
           {Topic(35),
            SemanticTreeNodeInformation(
                Topic(33),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_35)})},
           {Topic(36),
            SemanticTreeNodeInformation(
                Topic(33),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_36)})},
           {Topic(37),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_37)})},
           {Topic(38),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_38)})},
           {Topic(39),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_39)})},
           {Topic(40),
            SemanticTreeNodeInformation(
                Topic(23),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_40)})},
           {Topic(41),
            SemanticTreeNodeInformation(
                Topic(40),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_41)})},
           {Topic(42),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_42)})},
           {Topic(43),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_43)})},
           {Topic(44),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_44)})},
           {Topic(45),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_45)})},
           {Topic(46),
            SemanticTreeNodeInformation(
                Topic(45),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_46)})},
           {Topic(47),
            SemanticTreeNodeInformation(
                Topic(45),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_47)})},
           {Topic(48),
            SemanticTreeNodeInformation(
                Topic(45),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_48)})},
           {Topic(49),
            SemanticTreeNodeInformation(
                Topic(48),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_49)})},
           {Topic(50),
            SemanticTreeNodeInformation(
                Topic(45),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_50)})},
           {Topic(51),
            SemanticTreeNodeInformation(
                Topic(45),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_51)})},
           {Topic(52),
            SemanticTreeNodeInformation(
                Topic(45),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_52)})},
           {Topic(53),
            SemanticTreeNodeInformation(
                Topic(1),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_53)})},
           {Topic(54),
            SemanticTreeNodeInformation(
                Topic(53),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_54)})},
           {Topic(55),
            SemanticTreeNodeInformation(
                Topic(53),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_55)})},
           {Topic(56),
            SemanticTreeNodeInformation(
                Topic(53),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_56)})},
           {Topic(57),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_57)})},
           {Topic(58),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_58)})},
           {Topic(59),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_59)})},
           {Topic(60),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_60)})},
           {Topic(61),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_61)})},
           {Topic(62),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_62)})},
           {Topic(63),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_63)})},
           {Topic(64),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_64)})},
           {Topic(65),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_65)})},
           {Topic(66),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_66)})},
           {Topic(67),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_67)})},
           {Topic(68),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_68)})},
           {Topic(69),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_69)})},
           {Topic(70),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_70)})},
           {Topic(71),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_71)})},
           {Topic(72),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_72)})},
           {Topic(73),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_73)})},
           {Topic(74),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_74)})},
           {Topic(75),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_75)})},
           {Topic(76),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_76)})},
           {Topic(77),
            SemanticTreeNodeInformation(
                Topic(76),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_77)})},
           {Topic(78),
            SemanticTreeNodeInformation(
                Topic(62),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_78)})},
           {Topic(79),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_79)})},
           {Topic(80),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_80)})},
           {Topic(81),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_81)})},
           {Topic(82),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_82)})},
           {Topic(83),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_83)})},
           {Topic(84),
            SemanticTreeNodeInformation(
                Topic(83),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_84)})},
           {Topic(85),
            SemanticTreeNodeInformation(
                Topic(57),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_85)})},
           {Topic(86),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_86)})},
           {Topic(87),
            SemanticTreeNodeInformation(
                Topic(86),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_87)})},
           {Topic(88),
            SemanticTreeNodeInformation(
                Topic(86),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_88)})},
           {Topic(89),
            SemanticTreeNodeInformation(
                Topic(88),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_89)})},
           {Topic(90),
            SemanticTreeNodeInformation(
                Topic(88),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_90)})},
           {Topic(91),
            SemanticTreeNodeInformation(
                Topic(88),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_91)})},
           {Topic(92),
            SemanticTreeNodeInformation(
                Topic(88),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_92)})},
           {Topic(93),
            SemanticTreeNodeInformation(
                Topic(88),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_93)})},
           {Topic(94),
            SemanticTreeNodeInformation(
                Topic(88),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_94)})},
           {Topic(95),
            SemanticTreeNodeInformation(
                Topic(88),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_95)})},
           {Topic(96),
            SemanticTreeNodeInformation(
                Topic(86),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_96)})},
           {Topic(97),
            SemanticTreeNodeInformation(
                Topic(86),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_97)})},
           {Topic(98),
            SemanticTreeNodeInformation(
                Topic(97),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_98)})},
           {Topic(99),
            SemanticTreeNodeInformation(
                Topic(86),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_99)})},
           {Topic(100),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_100)})},
           {Topic(101),
            SemanticTreeNodeInformation(
                Topic(100),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_101)})},
           {Topic(102),
            SemanticTreeNodeInformation(
                Topic(100),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_102)})},
           {Topic(103),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_103)})},
           {Topic(104),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_104)})},
           {Topic(105),
            SemanticTreeNodeInformation(
                Topic(104),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_105)})},
           {Topic(106),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_106)})},
           {Topic(107),
            SemanticTreeNodeInformation(
                Topic(106),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_107)})},
           {Topic(108),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_108)})},
           {Topic(109),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_109)})},
           {Topic(110),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_110)})},
           {Topic(111),
            SemanticTreeNodeInformation(
                Topic(110),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_111)})},
           {Topic(112),
            SemanticTreeNodeInformation(
                Topic(110),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_112)})},
           {Topic(113),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_113)})},
           {Topic(114),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_114)})},
           {Topic(115),
            SemanticTreeNodeInformation(
                Topic(114),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_115)})},
           {Topic(116),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_116)})},
           {Topic(117),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_117)})},
           {Topic(118),
            SemanticTreeNodeInformation(
                Topic(117),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_118)})},
           {Topic(119),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_119)})},
           {Topic(120),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_120)})},
           {Topic(121),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_121)})},
           {Topic(122),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_122)})},
           {Topic(123),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_123)})},
           {Topic(124),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_124)})},
           {Topic(125),
            SemanticTreeNodeInformation(
                Topic(103),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_125)})},
           {Topic(126),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_126)})},
           {Topic(127),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_127)})},
           {Topic(128),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_128)})},
           {Topic(129),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_129)})},
           {Topic(130),
            SemanticTreeNodeInformation(
                Topic(129),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_130)})},
           {Topic(131),
            SemanticTreeNodeInformation(
                Topic(129),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_131)})},
           {Topic(132),
            SemanticTreeNodeInformation(
                Topic(129),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_132)})},
           {Topic(133),
            SemanticTreeNodeInformation(
                Topic(129),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_133)})},
           {Topic(134),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_134)})},
           {Topic(135),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_135)})},
           {Topic(136),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_136)})},
           {Topic(137),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_137)})},
           {Topic(138),
            SemanticTreeNodeInformation(
                Topic(137),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_138)})},
           {Topic(139),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_139)})},
           {Topic(140),
            SemanticTreeNodeInformation(
                Topic(126),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_140)})},
           {Topic(141),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_141)})},
           {Topic(142),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_142)})},
           {Topic(143),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_143)})},
           {Topic(144),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_144)})},
           {Topic(145),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_145)})},
           {Topic(146),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_146)})},
           {Topic(147),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_147)})},
           {Topic(148),
            SemanticTreeNodeInformation(
                Topic(140),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_148)})},
           {Topic(149),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_149)})},
           {Topic(150),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_150)})},
           {Topic(151),
            SemanticTreeNodeInformation(
                Topic(150),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_151)})},
           {Topic(152),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_152)})},
           {Topic(153),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_153)})},
           {Topic(154),
            SemanticTreeNodeInformation(
                Topic(153),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_154)})},
           {Topic(155),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_155)})},
           {Topic(156),
            SemanticTreeNodeInformation(
                Topic(155),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_156)})},
           {Topic(157),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_157)})},
           {Topic(158),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_158)})},
           {Topic(159),
            SemanticTreeNodeInformation(
                Topic(158),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_159)})},
           {Topic(160),
            SemanticTreeNodeInformation(
                Topic(158),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_160)})},
           {Topic(161),
            SemanticTreeNodeInformation(
                Topic(158),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_161)})},
           {Topic(162),
            SemanticTreeNodeInformation(
                Topic(158),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_162)})},
           {Topic(163),
            SemanticTreeNodeInformation(
                Topic(158),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_163)})},
           {Topic(164),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_164)})},
           {Topic(165),
            SemanticTreeNodeInformation(
                Topic(164),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_165)})},
           {Topic(166),
            SemanticTreeNodeInformation(
                Topic(164),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_166)})},
           {Topic(167),
            SemanticTreeNodeInformation(
                Topic(164),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_167)})},
           {Topic(168),
            SemanticTreeNodeInformation(
                Topic(164),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_168)})},
           {Topic(169),
            SemanticTreeNodeInformation(
                Topic(164),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_169)})},
           {Topic(170),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_170)})},
           {Topic(171),
            SemanticTreeNodeInformation(
                Topic(149),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_171)})},
           {Topic(172),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_172)})},
           {Topic(173),
            SemanticTreeNodeInformation(
                Topic(172),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_173)})},
           {Topic(174),
            SemanticTreeNodeInformation(
                Topic(173),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_174)})},
           {Topic(175),
            SemanticTreeNodeInformation(
                Topic(173),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_175)})},
           {Topic(176),
            SemanticTreeNodeInformation(
                Topic(175),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_176)})},
           {Topic(177),
            SemanticTreeNodeInformation(
                Topic(176),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_177)})},
           {Topic(178),
            SemanticTreeNodeInformation(
                Topic(173),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_178)})},
           {Topic(179),
            SemanticTreeNodeInformation(
                Topic(172),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_179)})},
           {Topic(180),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_180)})},
           {Topic(181),
            SemanticTreeNodeInformation(
                Topic(180),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_181)})},
           {Topic(182),
            SemanticTreeNodeInformation(
                Topic(180),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_182)})},
           {Topic(183),
            SemanticTreeNodeInformation(
                Topic(180),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_183)})},
           {Topic(184),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_184)})},
           {Topic(185),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_185)})},
           {Topic(186),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_186)})},
           {Topic(187),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_187)})},
           {Topic(188),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_188)})},
           {Topic(189),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_189)})},
           {Topic(190),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_190)})},
           {Topic(191),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_191)})},
           {Topic(192),
            SemanticTreeNodeInformation(
                Topic(183),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_192)})},
           {Topic(193),
            SemanticTreeNodeInformation(
                Topic(180),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_193)})},
           {Topic(194),
            SemanticTreeNodeInformation(
                Topic(180),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_194)})},
           {Topic(195),
            SemanticTreeNodeInformation(
                Topic(180),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_195)})},
           {Topic(196),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_196)})},
           {Topic(197),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_197)})},
           {Topic(198),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_198)})},
           {Topic(199),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_199)})},
           {Topic(200),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_200)})},
           {Topic(201),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_201)})},
           {Topic(202),
            SemanticTreeNodeInformation(
                Topic(201),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_202)})},
           {Topic(203),
            SemanticTreeNodeInformation(
                Topic(201),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_203)})},
           {Topic(204),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_204)})},
           {Topic(205),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_205)})},
           {Topic(206),
            SemanticTreeNodeInformation(
                Topic(196),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_206)})},
           {Topic(207),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_207)})},
           {Topic(208),
            SemanticTreeNodeInformation(
                Topic(207),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_208)})},
           {Topic(209),
            SemanticTreeNodeInformation(
                Topic(207),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_209)})},
           {Topic(210),
            SemanticTreeNodeInformation(
                Topic(207),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_210)})},
           {Topic(211),
            SemanticTreeNodeInformation(
                Topic(207),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_211)})},
           {Topic(212),
            SemanticTreeNodeInformation(
                Topic(207),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_212)})},
           {Topic(213),
            SemanticTreeNodeInformation(
                Topic(207),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_213)})},
           {Topic(214),
            SemanticTreeNodeInformation(
                Topic(207),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_214)})},
           {Topic(215),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_215)})},
           {Topic(216),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_216)})},
           {Topic(217),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_217)})},
           {Topic(218),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_218)})},
           {Topic(219),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_219)})},
           {Topic(220),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_220)})},
           {Topic(221),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_221)})},
           {Topic(222),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_222)})},
           {Topic(223),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_223)})},
           {Topic(224),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_224)})},
           {Topic(225),
            SemanticTreeNodeInformation(
                Topic(215),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_225)})},
           {Topic(226),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_226)})},
           {Topic(227),
            SemanticTreeNodeInformation(
                Topic(226),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_227)})},
           {Topic(228),
            SemanticTreeNodeInformation(
                Topic(227),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_228)})},
           {Topic(229),
            SemanticTreeNodeInformation(
                Topic(227),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_229)})},
           {Topic(230),
            SemanticTreeNodeInformation(
                Topic(227),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_230)})},
           {Topic(231),
            SemanticTreeNodeInformation(
                Topic(227),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_231)})},
           {Topic(232),
            SemanticTreeNodeInformation(
                Topic(231),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_232)})},
           {Topic(233),
            SemanticTreeNodeInformation(
                Topic(227),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_233)})},
           {Topic(234),
            SemanticTreeNodeInformation(
                Topic(227),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_234)})},
           {Topic(235),
            SemanticTreeNodeInformation(
                Topic(227),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_235)})},
           {Topic(236),
            SemanticTreeNodeInformation(
                Topic(226),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_236)})},
           {Topic(237),
            SemanticTreeNodeInformation(
                Topic(236),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_237)})},
           {Topic(238),
            SemanticTreeNodeInformation(
                Topic(236),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_238)})},
           {Topic(239),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_239)})},
           {Topic(240),
            SemanticTreeNodeInformation(
                Topic(239),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_240)})},
           {Topic(241),
            SemanticTreeNodeInformation(
                Topic(239),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_241)})},
           {Topic(242),
            SemanticTreeNodeInformation(
                Topic(241),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_242)})},
           {Topic(243),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_243)})},
           {Topic(244),
            SemanticTreeNodeInformation(
                Topic(243),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_244)})},
           {Topic(245),
            SemanticTreeNodeInformation(
                Topic(243),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_245)})},
           {Topic(246),
            SemanticTreeNodeInformation(
                Topic(243),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_246)})},
           {Topic(247),
            SemanticTreeNodeInformation(
                Topic(243),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_247)})},
           {Topic(248),
            SemanticTreeNodeInformation(
                Topic(243),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_248)})},
           {Topic(249),
            SemanticTreeNodeInformation(
                Topic(243),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_249)})},
           {Topic(250),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_250)})},
           {Topic(251),
            SemanticTreeNodeInformation(
                Topic(250),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_251)})},
           {Topic(252),
            SemanticTreeNodeInformation(
                Topic(250),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_252)})},
           {Topic(253),
            SemanticTreeNodeInformation(
                Topic(250),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_253)})},
           {Topic(254),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_254)})},
           {Topic(255),
            SemanticTreeNodeInformation(
                Topic(254),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_255)})},
           {Topic(256),
            SemanticTreeNodeInformation(
                Topic(255),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_256)})},
           {Topic(257),
            SemanticTreeNodeInformation(
                Topic(255),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_257)})},
           {Topic(258),
            SemanticTreeNodeInformation(
                Topic(255),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_258)})},
           {Topic(259),
            SemanticTreeNodeInformation(
                Topic(258),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_259)})},
           {Topic(260),
            SemanticTreeNodeInformation(
                Topic(258),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_260)})},
           {Topic(261),
            SemanticTreeNodeInformation(
                Topic(258),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_261)})},
           {Topic(262),
            SemanticTreeNodeInformation(
                Topic(254),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_262)})},
           {Topic(263),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_263)})},
           {Topic(264),
            SemanticTreeNodeInformation(
                Topic(263),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_264)})},
           {Topic(265),
            SemanticTreeNodeInformation(
                Topic(263),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_265)})},
           {Topic(266),
            SemanticTreeNodeInformation(
                Topic(265),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_266)})},
           {Topic(267),
            SemanticTreeNodeInformation(
                Topic(265),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_267)})},
           {Topic(268),
            SemanticTreeNodeInformation(
                Topic(265),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_268)})},
           {Topic(269),
            SemanticTreeNodeInformation(
                Topic(265),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_269)})},
           {Topic(270),
            SemanticTreeNodeInformation(
                Topic(265),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_270)})},
           {Topic(271),
            SemanticTreeNodeInformation(
                Topic(263),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_271)})},
           {Topic(272),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_272)})},
           {Topic(273),
            SemanticTreeNodeInformation(
                Topic(272),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_273)})},
           {Topic(274),
            SemanticTreeNodeInformation(
                Topic(272),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_274)})},
           {Topic(275),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_275)})},
           {Topic(276),
            SemanticTreeNodeInformation(
                Topic(275),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_276)})},
           {Topic(277),
            SemanticTreeNodeInformation(
                Topic(275),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_277)})},
           {Topic(278),
            SemanticTreeNodeInformation(
                Topic(275),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_278)})},
           {Topic(279),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_279)})},
           {Topic(280),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_280)})},
           {Topic(281),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_281)})},
           {Topic(282),
            SemanticTreeNodeInformation(
                Topic(281),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_282)})},
           {Topic(283),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_283)})},
           {Topic(284),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_284)})},
           {Topic(285),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_285)})},
           {Topic(286),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_286)})},
           {Topic(287),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_287)})},
           {Topic(288),
            SemanticTreeNodeInformation(
                Topic(279),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_288)})},
           {Topic(289),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_289)})},
           {Topic(290),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_290)})},
           {Topic(291),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_291)})},
           {Topic(292),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_292)})},
           {Topic(293),
            SemanticTreeNodeInformation(
                Topic(292),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_293)})},
           {Topic(294),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_294)})},
           {Topic(295),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_295)})},
           {Topic(296),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_296)})},
           {Topic(297),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_297)})},
           {Topic(298),
            SemanticTreeNodeInformation(
                Topic(289),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_298)})},
           {Topic(299),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_299)})},
           {Topic(300),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_300)})},
           {Topic(301),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_301)})},
           {Topic(302),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_302)})},
           {Topic(303),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_303)})},
           {Topic(304),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_304)})},
           {Topic(305),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_305)})},
           {Topic(306),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_306)})},
           {Topic(307),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_307)})},
           {Topic(308),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_308)})},
           {Topic(309),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_309)})},
           {Topic(310),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_310)})},
           {Topic(311),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_311)})},
           {Topic(312),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_312)})},
           {Topic(313),
            SemanticTreeNodeInformation(
                Topic(312),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_313)})},
           {Topic(314),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_314)})},
           {Topic(315),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_315)})},
           {Topic(316),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_316)})},
           {Topic(317),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_317)})},
           {Topic(318),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_318)})},
           {Topic(319),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_319)})},
           {Topic(320),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_320)})},
           {Topic(321),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_321)})},
           {Topic(322),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_322)})},
           {Topic(323),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_323)})},
           {Topic(324),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_324)})},
           {Topic(325),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_325)})},
           {Topic(326),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_326)})},
           {Topic(327),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_327)})},
           {Topic(328),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_328)})},
           {Topic(329),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_329)})},
           {Topic(330),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_330)})},
           {Topic(331),
            SemanticTreeNodeInformation(
                Topic(299),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_331)})},
           {Topic(332),
            SemanticTreeNodeInformation(
                absl::nullopt,
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_332)})},
           {Topic(333),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_333)})},
           {Topic(334),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_334)})},
           {Topic(335),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_335)})},
           {Topic(336),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_336)})},
           {Topic(337),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_337)})},
           {Topic(338),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_338)})},
           {Topic(339),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_339)})},
           {Topic(340),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_340)})},
           {Topic(341),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_341)})},
           {Topic(342),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_342)})},
           {Topic(343),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_343)})},
           {Topic(344),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_344)})},
           {Topic(345),
            SemanticTreeNodeInformation(
                Topic(344),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_345)})},
           {Topic(346),
            SemanticTreeNodeInformation(
                Topic(344),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_346)})},
           {Topic(347),
            SemanticTreeNodeInformation(
                Topic(344),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_347)})},
           {Topic(348),
            SemanticTreeNodeInformation(
                Topic(344),
                {SemanticTreeTaxonomyInformation(
                    1, IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_348)})},
           {Topic(349),
            SemanticTreeNodeInformation(
                Topic(332),
                {SemanticTreeTaxonomyInformation(
                    1,
                    IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_349)})}}));

  return *kSemanticTreeMap;
}

void GetDescendantTopicsHelper(
    const Topic& topic,
    const std::map<Topic, std::vector<Topic>>& parent_child_map,
    std::set<Topic>& result) {
  auto parent_child_it = parent_child_map.find(topic);
  if (parent_child_it == parent_child_map.end()) {
    return;
  }

  const std::vector<Topic>& child_topics = parent_child_it->second;
  for (const Topic& child_topic : child_topics) {
    if (result.contains(child_topic)) {
      continue;
    }
    result.insert(child_topic);
    GetDescendantTopicsHelper(child_topic, parent_child_map, result);
  }
}
}  // namespace

SemanticTree::SemanticTree() = default;
SemanticTree::~SemanticTree() = default;

void SemanticTree::InitializeParentToChildTopicMap() {
  std::map<Topic, std::vector<Topic>> parent_to_child_topic_map;
  for (const auto& [child, node_info] : GetSemanticTreeInternal()) {
    if (node_info.parent_topic.has_value()) {
      parent_to_child_topic_map[node_info.parent_topic.value()].emplace_back(
          child);
    }
  }
  parent_to_child_topic_map_.emplace(std::move(parent_to_child_topic_map));
}

std::set<Topic> SemanticTree::GetDescendantTopics(const Topic& topic) {
  if (!parent_to_child_topic_map_.has_value()) {
    InitializeParentToChildTopicMap();
  }
  std::set<Topic> descendant_topics;
  GetDescendantTopicsHelper(topic, parent_to_child_topic_map_.value(),
                            descendant_topics);
  return descendant_topics;
}

absl::optional<int> SemanticTree::GetLocalizedNameMessageId(
    const Topic& topic,
    int taxonomy_version) {
  base::fixed_flat_map<Topic, SemanticTreeNodeInformation,
                       kSemanticTreeSize>::const_iterator tree_node_it =
      GetSemanticTreeInternal().find(topic);
  if (tree_node_it == GetSemanticTreeInternal().end()) {
    return absl::nullopt;
  }
  for (const SemanticTreeTaxonomyInformation& taxonomy_info :
       tree_node_it->second.taxonomy_information) {
    if (taxonomy_info.taxonomy_version == taxonomy_version) {
      return taxonomy_info.localized_name_message_id;
    }
  }
  return absl::nullopt;
}
}  // namespace browsing_topics