# Measure Device Counts & Churn

## Contact Information

  * **Contact**:
    * hirthanan@google.com
    * qianwan@google.com
  * **Escalation**:
    * chromeos-data-eng@google.com
  * **File a bug**: [template](https://bugs.chromium.org/p/chromium/issues/list?q=component:OS%3ESoftware%3EData)


## Summary

We use a privacy compliant mechanism (private set membership) to
measure device counts and device churn.

The client deterministically generates a pseudonymous id using a high entropy
seed, which is used to send the pseudo-id at most once from the device.

In the future, there are plans to measure user level metrics.

## Developer Notes

### active_ts_ field - PLEASE NOTE DETAILS

Please note, we represent the device's online timestamp, adjusted to Pacific Time (PST)
in the variable |active_ts_|, which is a part of the UseCaseParameters object.

- Type: `base::Time`
- Initialization: Adjusted during initialization to Pacific time, based on Daylight savings.
- Usage: Propagated across child classes use cases (1DA, 28DA, Churn, etc.). `base::Time` methods will be PST-disguised-as-UTC-things (UTCExplode, UTCMidnight calls).

Purpose
- Compare dates in the Pacific Time zone to determine whether device pinged already.
- Determine new PST days.
- Store data in local and preserved file caches.
- Provides PST based metadata in network requests to Fresnel server, such as online PST day.