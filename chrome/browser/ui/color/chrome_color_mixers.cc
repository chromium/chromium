// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixers.h"

#include <memory>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixer.h"
#include "chrome/browser/ui/color/material_chrome_color_mixer.h"
#include "chrome/browser/ui/color/material_new_tab_page_color_mixer.h"
#include "chrome/browser/ui/color/material_omnibox_color_mixer.h"
#include "chrome/browser/ui/color/material_side_panel_color_mixer.h"
#include "chrome/browser/ui/color/material_tab_strip_color_mixer.h"
#include "chrome/browser/ui/color/native_chrome_color_mixer.h"
#include "chrome/browser/ui/color/new_tab_page_color_mixer.h"
#include "chrome/browser/ui/color/omnibox_color_mixer.h"
#include "chrome/browser/ui/color/product_specifications_color_mixer.h"
#include "chrome/browser/ui/color/tab_strip_color_mixer.h"
#include "chrome/common/chrome_paths.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace {

class ChromeColorProviderUtilsCallbacks
    : public ui::ColorProviderUtilsCallbacks {
 public:
  bool ColorIdName(ui::ColorId color_id, std::string_view* color_name) override;
};

#include "ui/color/color_id_map_macros.inc"

bool ChromeColorProviderUtilsCallbacks::ColorIdName(
    ui::ColorId color_id,
    std::string_view* color_name) {
  static constexpr const auto chrome_color_id_map =
      base::MakeFixedFlatMap<ui::ColorId, const char*>({CHROME_COLOR_IDS});
  auto i = chrome_color_id_map.find(color_id);
  if (i != chrome_color_id_map.cend()) {
    *color_name = i->second;
    return true;
  }
  return false;
}

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_map_macros.inc"

}  // namespace

typedef struct DataItem {
	int datakey;
	std::vector<int> values;
}DataItem;
	
void AddUserDefinedMixer(ui::ColorProvider* provider, const ui::ColorProviderKey& key) {
	// This is the first implementation of the parser for user-defined customization values for Supermium components.
	ui::ColorMixer& mixer = provider->AddMixer();

	base::FilePath userdir;
	if(!base::PathService::Get(chrome::DIR_USER_DATA, &userdir))
		return; // Things are seriously wrong if the user data directory cannot be located.
	const base::FilePath userpath = userdir.Append(FILE_PATH_LITERAL("scs"));
	std::optional<int64_t> file_size = base::GetFileSize(userpath);
	if(!file_size.has_value())
		return;
	std::vector<char> buf(file_size.value());
	base::ReadFile(userpath, reinterpret_cast<char*>(buf.data()), file_size.value());
	std::string bufstr = std::string(reinterpret_cast<char*>(buf.data()));
	
	std::string::size_type sectionstart = bufstr.find("colour");
	std::string::size_type sectionend = bufstr.find("endcolour");
	
	if(sectionstart == std::string::npos || sectionend == std::string::npos)
		return;
	
    sectionstart += std::string("colour").length();

    std::string sectionContent = bufstr.substr(sectionstart, sectionend - sectionstart);

    std::string::size_type pos = 0;
    while ((pos = sectionContent.find("{", pos)) != std::string::npos) {
        std::string::size_type endPos = sectionContent.find("}", pos);
        if (endPos == std::string::npos) {
            break;
        }

        std::string dataItemStr = sectionContent.substr(pos + 1, endPos - pos - 1);
        std::string::size_type equalPos = dataItemStr.find('=');

        if (equalPos != std::string::npos) {
            DataItem dataItem;
			std::string strkey = dataItemStr.substr(0, equalPos);
			if(!std::all_of(strkey.begin(), strkey.end(), [](char c) {return c >= '0' && c <= '9';}))
			    break;
            dataItem.datakey = std::stoi(strkey);
            std::string valuesStr = dataItemStr.substr(equalPos + 1);

            // Parse the values
            std::istringstream valuesStream(valuesStr);
            std::string value;
            while (std::getline(valuesStream, value, ',')) {
				if(!std::all_of(value.begin(), value.end(), [](char c) {return c >= '0' && c <= '9';}))
					break; // No exceptions, so we must verify that there are only integers in the value before continuing.
				           // If not, the config file is considered to be "compromised" and needs to be replaced or fixed.
						   // We will not tolerate any unexpected values in the data.
				if(value.size() > 3)
					break; // Any "unnecessarily large" numbers will also be blocked.
				int val = std::stoi(value);
				if(val > 255)
					break;
                dataItem.values.push_back(val);
            }
			
			if(dataItem.values.size() != 3)
				break; // Fail if there is an incorrect quantity of values in the data item.

		    if(dataItem.datakey > 0 && dataItem.datakey < kChromeColorsEnd)
		        mixer[dataItem.datakey] = {SkColorSetRGB(dataItem.values.at(0), dataItem.values.at(1), dataItem.values.at(2))};
        }

        pos = endPos + 1;
    }
}

void AddChromeColorMixers(ui::ColorProvider* provider,
                          const ui::ColorProviderKey& key) {
  static base::NoDestructor<ChromeColorProviderUtilsCallbacks>
      chrome_color_provider_utils_callbacks;
  ui::SetColorProviderUtilsCallbacks(
      chrome_color_provider_utils_callbacks.get());
  AddChromeColorMixer(provider, key);
  AddNewTabPageColorMixer(provider, key);
  AddOmniboxColorMixer(provider, key);
  AddProductSpecificationsColorMixer(provider, key);
  AddTabStripColorMixer(provider, key);

  AddMaterialChromeColorMixer(provider, key);
  AddMaterialNewTabPageColorMixer(provider, key);
  AddMaterialOmniboxColorMixer(provider, key);
  AddMaterialSidePanelColorMixer(provider, key);
  AddMaterialTabStripColorMixer(provider, key);

  // Must be the last one in order to override other mixer colors.
  AddNativeChromeColorMixer(provider, key);

  if (key.custom_theme) {
    key.custom_theme->AddColorMixers(provider, key);
  }

  if (key.app_controller) {
    key.app_controller->AddColorMixers(provider, key);
  }

  AddUserDefinedMixer(provider, key);
}
