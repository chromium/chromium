# ManagementService

ManagementService is an abstract class that exposes an interface to get an
entity's management state.

This class allows the consumer to find out if and EnterpriseManagementAuthority
is actively exercising management.

This class also allows the consumer to get the highest level of
trustworthiness of all the active management authorities.

## ManagementStatusProvider
This is an abstract class used to get the management status of a single entity.
This should return the appropriate EnterpriseManagementAuthority.

### Caching

Some management status result is cached due to system API performance. It means we will return stale data during Chrome launch process.

## BrowserManagementService

A specialization of ManagementService that returns management information on the
browser itself. This returns info solely on the browser's management regardless
of the OS or device management since the OS or device could be managed without
the browser being managed.
- The browser is managed if there are policies being applied on the browser
- The browser is managed is the primary signed in account comes from a managed
domain.
- The browser is not managed if the device is managed but no policy is applied
 on the browser.
- The browser is managed if the device is enrolled in CBCM.

**Usage** This class' lifetime is bound to a Profile as a KeyedService and must
be called from the UI thread at all times.
Use `policy::ManagementServiceFactory::GetForProfile()` to access this class.

## PlatformManagementService

A specialization of ManagementService that returns management information on the
OS or device. This returns info solely on the OS or device management regardless
of the browser.
- The OS is managed if it is domain joined or an enterprise version.
- The device is managed if it is registered as an enterprise device.
- The OS nor the device are necessarily managed if CBCM is active.

**Usage** This class is a singleton and must be called from the
UI thread at all times. It is recommended to use
`policy::ManagementServiceFactory::GetForPlatform()` to access this class.
`policy::PlatformManagementService::GetInstance()` should only be used outside of
 //chrome/*.

## EnterpriseManagementAuthority

An enterprise management authority is an enum used to classify an entity that
can exercise management.

**NONE** No active entity is exercising management.

**COMPUTER_LOCAL** An entity exercising management from the computer itself.
This type of entity has a low level of trust. This means that this type of
management has a high chance of not coming from an enterprise.
i.e. : Policies set locally by an admin (GPO, linux JSON policies)

**DOMAIN_LOCAL** An entity exercising management from the computer and tied to
a local domain. This type of entity has a high level of trust. This means that
this type of management has a low chance of not coming from an enterprise.
i.e. : Computer is Active Directory Joined, Puppet

**CLOUD** An entity exercising management from a cloud source.
This type of entity has a high level of trust. This means that this type of
management has a low chance of not coming from an enterprise.
i.e. : MDM management, GSuite User

**CLOUD_DOMAIN** An entity exercising management from a cryptographically
verifiable cloud source. This type of entity has the highest level of trust.
This means that this type of management has a very low chance of not coming from
an enterprise.
i.e. : Azure Active directory, CBCM

The presence of `CLOUD` and/or `CLOUD_DOMAIN` in `BrowserManagementService` may
be used to determine that the browser is managed by a Google product such as
GSuite or CBCM.

The presence of `CLOUD` and/or `CLOUD_DOMAIN` in `PlatformManagementService` may
be used to determine that the platform is managed by a Google product only on
ChromeOS.

## ManagementAuthorityTrustworthiness

This enum is used to rank the level of trustworthiness of the
management authorities.

**NONE** No management authority was found.

**LOW** Local device management authority.

**TRUSTED** Non-local management authority.

**FULLY_TRUSTED** Cryptographically verifiable policy source.

# ManagementStatusProvider
This is an abstract class used to get the management status of a single entity.
This should return the appropriate EnterpriseManagementAuthority.