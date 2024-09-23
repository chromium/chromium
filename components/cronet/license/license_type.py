from enum import Enum


class LicenseType(Enum):
  # The higher the value, the higher the restrictions.
  UNKNOWN = 0,
  UNENCUMBERED = 1,
  PERMISSIVE = 2,
  NOTICE = 3,
  RECIPROCAL = 4,
  RESTRICTED = 5,
  BY_EXCEPTION_ONLY = 6
