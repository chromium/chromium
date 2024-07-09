# Add new permission

[TOC]

Make sure to notify permissions-core@ when you plan to add a new permission.

### PermissionType vs ContentSettingsType

[PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
roughly matches the web-facing APIs for Permission Policy and Permissions API.

[ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h)
is a key used in
[HostContentSettingsMap](https://cs.chromium.org/chromium/src/components/content_settings/core/browser/host_content_settings_map.h)
to store per-type and per-origin JSON dictionaries. It contains all entries from
[PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
but additionally, it contains entries that are not permissions, e.g. Cookies,
Images, Javascript, Sound, etc.

[ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h)
represents the Chrome-layer entity and is not available outside of Chrome. We
use
[PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
as a Web Platform - layer list of permissions. That means we always need to have
a pair of
[PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
[ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h).
To improve clarity of the permissions API we’re trying to use
[PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
everywhere it is possible. Conceptually
[ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h)
should be used only in
[HostContentSettingsMap](https://cs.chromium.org/chromium/src/components/content_settings/core/browser/host_content_settings_map.h).
But there are still a few places where
[ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h)
is presented.

* [PermissionContextBase](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.h)
* [PermissionManager](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_manager.h;drc=38bff96f36fb3980fef273c1066ba65a7b32da7c;l=44)
* [PermissionRequestManager](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request_manager.h)
* [PermissionRequest](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request.h)
* [PermissionManagerFactory](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/permission_manager_factory.h)

We’re actively working on reducing [ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h)
footprint in the permissions code base.

We have a few utils methods in components/permissions/permission_util.h that
convert [PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
to
[ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h)
and vice versa. We strongly encourage developers to use
[PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb).

### PermissionContexts

[PermissionContextBase](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.h)
provides default implementation for requesting/verifying and saving a permission
state. Most of the methods in the class are virtual that allows for a newly
added permission to customize how it can be requested, verified and stored. A
custom implementation of the
[PermissionContextBase](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.h)
is the only way to init permission policy. For most of the cases you do not need
to change the default implementation. A good example of the custom behavior is
an integration with OS-level permissions. E.g. Notifications permission on
Android has custom logic that verifies if Chrome has app-level permission and if
a notification channel exists, additionally Notifications has custom logic for
extensions. Similarly Geolocation has multiple implementations for different
platforms
[GeolocationPermissionContext](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/contexts/geolocation_permission_context.h).

## Add new permission
### Add PermissionType
1. Add a new enum entry to [PermissionType](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;l=23;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
2. Update [permission_descriptor.idl](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/permissions/permission_descriptor.idl)
3. In  [permission_utils.cc](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/permissions/permission_utils.cc)
update methods:
    * [PermissionNameToString()](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/permissions/permission_utils.cc;l=56;drc=5a1dda5d1bf6b67b478b2d35dc478508318213cf)
    * [ParsePermissionDescriptor()](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/permissions/permission_utils.cc;l=148;drc=5a1dda5d1bf6b67b478b2d35dc478508318213cf)
4. Update [permission.mojom](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/permissions/permission.mojom)
5. In [permission_utils.cc](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/permissions/permission_utils.cc)
update
    * [GetPermissionString()](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/permissions/permission_utils.cc;l=28;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
    * [PermissionDescriptorInfoToPermissionType()](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/permissions/permission_utils.cc;l=126;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
6. In [permission_controller_impl.cc](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/permission_controller_impl.cc)
update [PermissionToSchedulingFeature()](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/permissions/permission_controller_impl.cc;l=31;drc=a989b8968ead0d1a80b285eef0a2acf9feb71c82)
. By default the new feature should return `std::nullopt`. That means it will
not block back-forward cache. If your feature needs to block it, make sure it is
explicitly mentioned in a design doc as that needs to be reviewed.
7. In [aw_permission_manager.cc](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/aw_permission_manager.cc)
update
    * [RequestPermissions()](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/aw_permission_manager.cc;l=252;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
    * [CancelPermissionRequest()](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/aw_permission_manager.cc;l=514;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
8. In [shell_permission_manager.cc](https://source.chromium.org/chromium/chromium/src/+/main:content/shell/browser/shell_permission_manager.cc)
update [IsAllowlistedPermissionType()](https://source.chromium.org/chromium/chromium/src/+/main:content/shell/browser/shell_permission_manager.cc;l=24;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)

### Add ContentSettingType
1. In [content_settings_types.h](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h)
update
[ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h;l=17;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
2. In [content_settings_uma_util.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_uma_util.cc)
update
[kHistogramValue](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_uma_util.cc;drc=c6239d599e27bb680984bd6e86e69321b3fe5a9d;l=16)
3. In [content_settings_registry.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_registry.cc)
update method
[ContentSettingsRegistry::Init()](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_registry.cc;l=122;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
    * Register the new entry as
    [WebsiteSettingsInfo::UNSYNCABLE](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/website_settings_info.h;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0;l=19)
    . If you need to use
    [WebsiteSettingsInfo::SYNCABLE](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/website_settings_info.h;l=23;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
    , then it should be explicitly mentioned in a design doc.
4. In [permission_util.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_util.cc)
update
    * [GetPermissionDelegationMode()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_util.cc;l=49;drc=334d00e7293fa3a7127a6270f0c5720d5e46a7eb)
. If you need non-default permission delegation logic. That should be explicitly stated in a design doc.
    * [GetPermissionType()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_util.cc;l=78;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
    * [PermissionTypeToContentSettingTypeSafe()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_util.cc;l=229;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
5. In [WebsitePermissionsFetcherTest.java](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/site_settings/WebsitePermissionsFetcherTest.java) update [assertion](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/site_settings/WebsitePermissionsFetcherTest.java;l=516;drc=c08fa134ac65f3bec52794d30281c98d18c7aa0f).

### UI-less ContentSettingType
If you add a new [ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h;l=17;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
which require no UI and no associated permission, then:

1. In [site_settings_helper.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/settings/site_settings_helper.cc) update
[kContentSettingsTypeGroupNames](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/settings/site_settings_helper.cc;l=78;drc=a58357a4ec6a0ebc1630fe2504974273e44d2814).
    * Add a new entry into the second part of the array.

### PageInfo, Settings Page

Some [ContentSettingsType](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/content_settings_types.h;l=17;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc) is displayed in `chrome://settings/content`. To be able to display it there, you need to do:

1. In [site_settings_helper.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/settings/site_settings_helper.cc) update [kContentSettingsTypeGroupNames](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/settings/site_settings_helper.cc;l=78;drc=a58357a4ec6a0ebc1630fe2504974273e44d2814).
    * Add a new entry into the first part of the array as permissions are
    available in the content settings page.
2. Add new strings for the new permission. The strings should be reviewed by
meggynwatkins@ and permissions-core@ in a design doc.
3. In [page_info.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/page_info/page_info.cc)
update [kPermissionType](https://source.chromium.org/chromium/chromium/src/+/main:components/page_info/page_info.cc;l=89;drc=796b971e6e0d1b033bda58b31e9f24d397ad6178)
. `IMPORTANT`: The order in the array is not random or organic. Please carefully
read [the comment](https://source.chromium.org/chromium/chromium/src/+/main:components/page_info/page_info.cc;l=86-88;drc=796b971e6e0d1b033bda58b31e9f24d397ad6178).
4. In [page_info_ui.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/page_info/page_info_ui.cc)
update methods:
    * [GetContentSettingsUIInfo()](https://source.chromium.org/chromium/chromium/src/+/main:components/page_info/page_info_ui.cc;l=132;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
    * [GetPermissionAskStateString()](https://source.chromium.org/chromium/chromium/src/+/main:components/page_info/page_info_ui.cc;l=299;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
5. Add a new icon for the permission to be displayed in PageInfo
6. In [page_info_view_factory.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/page_info/page_info_view_factory.cc) update
    * [GetPermissionIcon()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/page_info/page_info_view_factory.cc;l=246;drc=497b76a1ed8d0ba3a870484cbd1f34a1392fedf6)

### Permission request
Most permissions allow sites to request a permission prompt where a user can grant or deny the permission.

1. In [request_type.h](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/request_type.h)
update enum [RequestType](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/request_type.h;l=22;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
2. In [request_type.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/request_type.cc) update
    * [GetIconId*()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/request_type.cc;l=269;drc=c970b8647281cc3fc7ffe1288ce7c5f2e5f7acf6) for Android and Desktop
        * Reuse icons from the step above.
    * [GetDialogMessageText()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request.cc;l=44;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0) for Android
    * [GetMessageTextFragment()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_request.cc;l=166;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0) for Desktop
        * Permission prompt strings should be reviewed by meggynwatkins@ and permissions-core@ in a design doc.
    * [PermissionKeyForRequestType()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/request_type.cc;l=288;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
    * [GetBlockedIconIdDesktop()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/request_type.cc;l=116;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)

### Tracking
1. In [tools/metrics/histograms/enums.xml](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/enums.xml)
update histograms:
    * `name="PermissionType"`
    * `name="ContentType"`
    * `name="PermissionRequestType"`
2. In  [tools/metrics/histograms/metadata/histogram_suffixes_list.xml](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/histogram_suffixes_list.xml)
update histograms
    * `name="PermissionTypes"`
    * `name="PermissionRequestTypes"`
3. In [tools/metrics/histograms/metadata/content/histograms.xml](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/content/histograms.xml) update `<token key="ContentSettingsType">`
4. In [permission_uma_util.h](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_uma_util.h)
updated enum
[RequestTypeForUma](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_uma_util.h;l=41;drc=796b971e6e0d1b033bda58b31e9f24d397ad6178)
5. In [permission_uma_util.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_uma_util.cc)
updated methods:
    * [GetUmaValueForRequestType()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_uma_util.cc;l=55;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
    * [GetPermissionRequestString()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_uma_util.cc;l=117;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
    * [RecordPermissionAction()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_uma_util.cc;l=843-914;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)

### Extend PermissionContextBase
To extend [PermissionContextBase](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.h):

1. Create a new class `<PermissionName>PermissionContext` and extend
[permissions::PermissionContextBase](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.h).
2. Put it in `components/permissions/contexts/` or in a feature folder.
3. In [permission_manager_factory.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/permission_manager_factory.cc) update [CreatePermissionContexts()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/permission_manager_factory.cc;l=50;drc=c970b8647281cc3fc7ffe1288ce7c5f2e5f7acf6)
4. In [permission_manager_factory.cc](https://source.chromium.org/chromium/chromium/src/+/main:weblayer/browser/permissions/permission_manager_factory.cc) update [CreatePermissionContexts()](https://source.chromium.org/chromium/chromium/src/+/main:weblayer/browser/permissions/permission_manager_factory.cc;l=61;drc=0e1f7e00e6ee851f977f5f37d711ee5d4e9b28eb)
5. In [permission_test_util.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/test/permission_test_util.cc) update [CreatePermissionContexts()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/test/permission_test_util.cc;l=46;drc=0e1f7e00e6ee851f977f5f37d711ee5d4e9b28eb)

### DevTools
Add new permissions for browser automation (e2e testing).

1. Update [third_party/blink/public/devtools_protocol/browser_protocol.pdl](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/devtools_protocol/browser_protocol.pdl)
2. In [content/browser/devtools/protocol/browser_handler.cc](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/browser_handler.cc)
update
    * [PermissionDescriptorToPermissionType()](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/browser_handler.cc;l=152;drc=f7065e23ad878a5f85f9c43eff6c0381474db2fe)
    * [FromProtocolPermissionType()](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/browser_handler.cc;l=223;drc=f7065e23ad878a5f85f9c43eff6c0381474db2fe)


### Optional: Add a permission policy
If you need to add a permission policy:

1. In [permissions_policy_feature.mojom](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom)
update enum
[PermissionsPolicyFeature](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom;l=16;drc=272becfe736870f18b79355d73475a5eb86c93b1)
2. In [permissions_policy_features.json5](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/permissions_policy/permissions_policy_features.json5)
update `data` array with the new policy.
5. Update [feature-policy-features-expected.txt](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt)
6. In the `<PermissionName>PermissionContext`, make sure to initialize the permission policy variable
[permissions_policy_feature_](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.h;l=223;drc=caa1747121ee9f14ba7d4e346ea2dc5e7a2e05c0)
in the class ctor.

### Optional: Enterprise policies
Permissions infrastructure has its own provider for storing policies
[PolicyProvider](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_policy_provider.h).
If you plan on allowing enterprise admins to control your newly added
permission:

1. Follow that instructions [docs/enterprise/add_new_policy.md](https://source.chromium.org/chromium/chromium/src/+/main:docs/enterprise/add_new_policy.md#adding-a-new-policy)
2. For the [step #6](https://source.chromium.org/chromium/chromium/src/+/main:docs/enterprise/add_new_policy.md;l=98-111;drc=cd66a348b90e5260b5ff0ac6da36ef99e9b328f9)
you need to create and register variables in content settings policy providers.
That allows automatically verifying policy values for a given permission.
    * Create a new policy in
    [components/content_settings/core/common/pref_names.(h/cc)](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/pref_names.h)
    * In
    [content_settings_policy_provider.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/common/pref_names.cc)
update
        * [PrefsForManagedContentSettingsMapEntry](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_policy_provider.cc;l=35;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
        * [kManagedPrefs](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_policy_provider.cc;l=112;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
        * [kManagedDefaultPrefs](https://source.chromium.org/chromium/chromium/src/+/main:components/content_settings/core/browser/content_settings_policy_provider.cc;l=159;drc=0c2e6d2e27af976e1b28eebd7dacc7a0296bb1cc)
    * In ​[configuration_policy_handler_list_factory.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/policy/configuration_policy_handler_list_factory.cc)
    register the newly created policies as well.
3. In [tools/metrics/histograms/enums.xml](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/enums.xml)
add new policies in `enum name="EnterprisePolicies"`
