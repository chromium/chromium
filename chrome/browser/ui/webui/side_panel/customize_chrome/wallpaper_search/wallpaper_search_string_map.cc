// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_string_map.h"

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr auto kCategoryStringMap =
    base::MakeFixedFlatMap<std::string_view, int>({
        {"Desserts", IDS_NTP_WALLPAPER_SEARCH_CATEGORY_DESSERTS},
        {"Interiors", IDS_NTP_WALLPAPER_SEARCH_CATEGORY_INTERIORS},
        {"Landscape", IDS_NTP_WALLPAPER_SEARCH_CATEGORY_LANDSCAPE},
        {"Nature", IDS_NTP_WALLPAPER_SEARCH_CATEGORY_NATURE},
        {"Outer space", IDS_NTP_WALLPAPER_SEARCH_CATEGORY_OUTER_SPACE},
        {"Places", IDS_NTP_WALLPAPER_SEARCH_CATEGORY_PLACES},
        {"Structures", IDS_NTP_WALLPAPER_SEARCH_CATEGORY_STRUCTURES},
    });

constexpr auto kDescriptorAStringMap =
    base::MakeFixedFlatMap<std::string_view, int>({
        {"Asteroid", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_ASTEROID},
        {"Aurora borealis", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_AURORA_BOREALIS},
        {"Brick Building", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_BRICK_BUILDING},
        {"Cake pops", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CAKE_POPS},
        {"Castle", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CASTLE},
        {"Cheesecake", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CHEESECAKE},
        {"Chemistry lab", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CHEMISTRY_LAB},
        {"Chichen Itza", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CHICHEN_ITZA},
        {"Cliff", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CLIFF},
        {"Comet", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_COMET},
        {"Crashing waves", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CRASHING_WAVES},
        {"Creek", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_CREEK},
        {"Desert", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_DESERT},
        {"Eclipses", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_ECLIPSES},
        {"Flowers", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_FLOWERS},
        {"Forest", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_FOREST},
        {"Fruit tart", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_FRUIT_TART},
        {"Grand Canyon", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_GRAND_CANYON},
        {"Grand Teton National Park",
         IDS_NTP_WALLPAPER_SEARCH_SUBJECT_GRAND_TETON_NATIONAL_PARK},
        {"Great Pyramid of Giza",
         IDS_NTP_WALLPAPER_SEARCH_SUBJECT_GREAT_PYRAMID_OF_GIZA},
        {"Hallway", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_HALLWAY},
        {"Highlands", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_HIGHLANDS},
        {"Highway", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_HIGHWAY},
        {"House", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_HOUSE},
        {"Ice cream", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_ICE_CREAM},
        {"Kitchen", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_KITCHEN},
        {"Lighthouse", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_LIGHTHOUSE},
        {"Lightning", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_LIGHTNING},
        {"Living room", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_LIVING_ROOM},
        {"Macaron", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_MACARON},
        {"Meadow", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_MEADOW},
        {"Moon", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_MOON},
        {"Mountain", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_MOUNTAIN},
        {"Nebula", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_NEBULA},
        {"Night sky", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_NIGHT_SKY},
        {"Ocean", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_OCEAN},
        {"Palm tree", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_PALM_TREE},
        {"Panna cotta", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_PANNA_COTTA},
        {"Pier", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_PIER},
        {"Planet", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_PLANET},
        {"Rainbow", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_RAINBOW},
        {"Rainforest", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_RAINFOREST},
        {"Reading nook", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_READING_NOOK},
        {"Rice paddy", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_RICE_PADDY},
        {"River", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_RIVER},
        {"Rocky Mountains National Park",
         IDS_NTP_WALLPAPER_SEARCH_SUBJECT_ROCKY_MOUNTAINS_NATIONAL_PARK},
        {"Salt flat", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_SALT_FLAT},
        {"Shoreline", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_SHORELINE},
        {"Slot canyon", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_SLOT_CANYON},
        {"Space station", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_SPACE_STATION},
        {"Stars", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_STARS},
        {"Sunset", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_SUNSET},
        {"The Colosseum", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_THE_COLOSSEUM},
        {"Tundra", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_TUNDRA},
        {"Waterfall", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_WATERFALL},
        {"Zion National Park",
         IDS_NTP_WALLPAPER_SEARCH_SUBJECT_ZION_NATIONAL_PARK},
    });

constexpr auto kDescriptorBStringMap =
    base::MakeFixedFlatMap<std::string_view, int>({
        {"Animated", IDS_NTP_WALLPAPER_SEARCH_STYLE_ANIMATED},
        {"Colored Pencil", IDS_NTP_WALLPAPER_SEARCH_STYLE_COLORED_PENCIL},
        {"Cyberpunk", IDS_NTP_WALLPAPER_SEARCH_STYLE_CYBERPUNK},
        {"Dream", IDS_NTP_WALLPAPER_SEARCH_STYLE_DREAM},
        {"Expressionism", IDS_NTP_WALLPAPER_SEARCH_STYLE_EXPRESSIONISM},
        {"Fantasy", IDS_NTP_WALLPAPER_SEARCH_STYLE_FANTASY},
        {"Impressionism", IDS_NTP_WALLPAPER_SEARCH_STYLE_IMPRESSIONISM},
        {"Oil Painting", IDS_NTP_WALLPAPER_SEARCH_STYLE_OIL_PAINTING},
        {"Organic", IDS_NTP_WALLPAPER_SEARCH_STYLE_ORGANIC},
        {"Photography", IDS_NTP_WALLPAPER_SEARCH_STYLE_PHOTOGRAPHY},
        {"Splatter", IDS_NTP_WALLPAPER_SEARCH_STYLE_SPLATTER},
        {"Steampunk", IDS_NTP_WALLPAPER_SEARCH_STYLE_STEAMPUNK},
        {"Watercolor", IDS_NTP_WALLPAPER_SEARCH_STYLE_WATERCOLOR},
    });

constexpr auto kDescriptorCStringMap =
    base::MakeFixedFlatMap<std::string_view, int>({
        {"Chaotic", IDS_NTP_WALLPAPER_SEARCH_MOOD_CHAOTIC},
        {"Creative", IDS_NTP_WALLPAPER_SEARCH_MOOD_CREATIVE},
        {"Dark", IDS_NTP_WALLPAPER_SEARCH_MOOD_DARK},
        {"Dreamy", IDS_NTP_WALLPAPER_SEARCH_MOOD_DREAMY},
        {"Excited", IDS_NTP_WALLPAPER_SEARCH_MOOD_EXCITED},
        {"Happy", IDS_NTP_WALLPAPER_SEARCH_MOOD_HAPPY},
        {"Intellectual", IDS_NTP_WALLPAPER_SEARCH_MOOD_INTELLECTUAL},
        {"Orderly", IDS_NTP_WALLPAPER_SEARCH_MOOD_ORDERLY},
        {"Romantic", IDS_NTP_WALLPAPER_SEARCH_MOOD_ROMANTIC},
        {"Serene", IDS_NTP_WALLPAPER_SEARCH_MOOD_SERENE},
        {"Snowy", IDS_NTP_WALLPAPER_SEARCH_MOOD_SNOWY},
        {"Subdued", IDS_NTP_WALLPAPER_SEARCH_MOOD_SUBDUED},
        {"Sunny", IDS_NTP_WALLPAPER_SEARCH_MOOD_SUNNY},
        {"Thoughtful", IDS_NTP_WALLPAPER_SEARCH_MOOD_THOUGHTFUL},
        {"Whimsical", IDS_NTP_WALLPAPER_SEARCH_MOOD_WHIMSICAL},
        {"Wild", IDS_NTP_WALLPAPER_SEARCH_MOOD_WILD},
    });

constexpr auto kInspirationDescriptionStringMap =
    base::MakeFixedFlatMap<std::string_view, int>({
        {"DD924B31C087A231A481FEF889BE48EC",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_1},
        {"6E1177B1DD94E22E4BBE01B647CECF31",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_2},
        {"580FA15D4970450999CDBE222E38A904",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_3},
        {"BE56B56CDCF9F362AD4D8D0FB5998B1A",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_4},
        {"083F73C24B75E478E8CFE6ABE599F9AF",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_5},
        {"6E0D5E1E40A9727C5472C41961412CE0",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_6},
        {"2A9F4D1E94CC1AF029C3E84D0C0DB6A1",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_7},
        {"02D5D717E472DC3095611B7839C07944",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_8},
        {"011590ACB4597DAF1EDEEBD5237841C5",
         IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_DESCRIPTION_9},
    });

template <size_t N>
std::optional<std::string> FindString(
    std::string_view key,
    const base::fixed_flat_map<std::string_view, int, N>& map) {
  auto it = map.find(key);
  return it != map.end()
             ? std::make_optional(l10n_util::GetStringUTF8(it->second))
             : std::nullopt;
}

WallpaperSearchStringMap::Factory& GetFactoryInstance() {
  static base::NoDestructor<WallpaperSearchStringMap::Factory> instance;
  return *instance;
}
}  // namespace

// static
std::unique_ptr<WallpaperSearchStringMap> WallpaperSearchStringMap::Create() {
  if (GetFactoryInstance()) {
    return GetFactoryInstance().Run();
  }
  return base::WrapUnique(new WallpaperSearchStringMap());
}

// static
void WallpaperSearchStringMap::SetFactory(Factory factory) {
  GetFactoryInstance() = factory;
}

std::optional<std::string> WallpaperSearchStringMap::FindCategory(
    std::string_view key) const {
  return FindString(key, kCategoryStringMap);
}

std::optional<std::string> WallpaperSearchStringMap::FindDescriptorA(
    std::string_view key) const {
  return FindString(key, kDescriptorAStringMap);
}

std::optional<std::string> WallpaperSearchStringMap::FindDescriptorB(
    std::string_view key) const {
  return FindString(key, kDescriptorBStringMap);
}

std::optional<std::string> WallpaperSearchStringMap::FindDescriptorC(
    std::string_view key) const {
  return FindString(key, kDescriptorCStringMap);
}

std::optional<std::string> WallpaperSearchStringMap::FindInspirationDescription(
    std::string_view key) const {
  return FindString(key, kInspirationDescriptionStringMap);
}
