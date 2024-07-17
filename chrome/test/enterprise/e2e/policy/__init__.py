# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .allow_deleting_browser_history.allow_deleting_browser_history import *
from .apps_shortcut.apps_shortcut import *
from .bookmarkbar_enabled.bookmarkbar_enabled import *
from .cloud_management_enrollment_token.cloud_management_enrollment_token import *
from .cloud_reporting_enabled.cloud_reporting_enabled import *
from .default_search_provider.default_search_provider import *
from .extension_blocklist.extension_blocklist import *
from .extension_forcelist.extension_forcelist import *
from .extension_allowlist.extension_allowlist import *
from .force_google_safe_search.force_google_safe_search import *
# Disable fullscreenallowed test due to pywinauto infra issue http://b/259118140
# from .fullscreen_allowed.fullscreen_allowed import *
from .homepage.homepage import *
from .mergelist.mergelist import *
from .password_manager_enabled.password_manager_enabled import *
from .popups_allowed.popups_allowed import *
from .precedence.precedence import *
from .encrypted_reporting.report_cbcm_events import *
from .restore_on_startup.restore_on_startup import *
# Disable safe_browsing test due to chrome://downloads shadow DOM issue http://b/298889715
# from .safe_browsing.safe_browsing import *
from .translate_enabled.translate_enabled import *
from .url_blocklist.url_blocklist import *
from .url_allowlist.url_allowlist import *
from .user_data_dir.user_data_dir import *
from .webprotect_file_download.webprotect_file_download import *
from .youtube_restrict.youtube_restrict import *
