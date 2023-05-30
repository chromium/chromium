# Measure Device Counts & Churn

We use a privacy compliant mechanism (private set membership) to
measure device counts and device churn.

The client deterministically generates a pseudonymous id using a high entropy
seed, which is used to send the pseudo-id at most once from the device.

In the future, there are plans to measure user level metrics.

Googlers: See go/chromeos-data-pc
POC: hirthanan@, qianwan@, nsinghal@
Team: chromeos-data-eng@
