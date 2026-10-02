# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Computes and records media perf metrics."""

import csv
import logging
import os
import re
import subprocess

import perf_config

# pylint: disable=import-error, wrong-import-position
from lib.proto import measures
from lib.results import result_sink
# pylint: enable=import-error, wrong-import-position


def calculate_psnr_ssim(
    video_file: str, recorded_path: str, original_path: str
):
    """Calculates PSNR and SSIM via FFmpeg and records them."""
    logging.info("Calculating PSNR and SSIM via FFmpeg...")

    # PSNR command to compare the recorded video against the original reference.
    # Args:
    # - '-i': Input files (recorded_path, original_path).
    # - '-lavfi': Libavfilter graph.
    # - '[0:v][1:v]scale2ref[rec][orig]': Scale the recorded video (0:v) to
    #   match the reference video (1:v) resolution.
    # - '[rec][orig]psnr': Calculate PSNR on the scaled videos.
    # - '-f null -': Force null output (don't save a file, just output stats).
    psnr_cmd = [
        'ffmpeg',
        '-i',
        recorded_path,
        '-i',
        original_path,
        '-lavfi',
        '[0:v][1:v]scale2ref[rec][orig];[rec][orig]psnr',
        '-f',
        'null',
        '-',
    ]
    try:
        psnr_result = subprocess.run(
            psnr_cmd, capture_output=True, text=True, timeout=120, check=False
        )
        psnr_match = re.search(r'average:(\d+\.\d+|inf)', psnr_result.stderr)
        if psnr_match:
            val = psnr_match.group(1)
            psnr_val = 100.0 if val == 'inf' else float(val)
            measures.average(video_file, 'video_perf', 'psnr').record(psnr_val)
            logging.info("PSNR: %s", val)
        else:
            logging.warning(
                "Failed to parse PSNR from FFmpeg output. Stderr: %s",
                psnr_result.stderr,
            )
    except Exception as e:  # pylint: disable=broad-exception-caught
        logging.error("Failed to calculate PSNR: %s", e)

    # SSIM command. Arguments are identical to PSNR above, but calculates
    # Structural Similarity (SSIM) instead.
    ssim_cmd = [
        'ffmpeg',
        '-i',
        recorded_path,
        '-i',
        original_path,
        '-lavfi',
        '[0:v][1:v]scale2ref[rec][orig];[rec][orig]ssim',
        '-f',
        'null',
        '-',
    ]
    try:
        ssim_result = subprocess.run(
            ssim_cmd, capture_output=True, text=True, timeout=120, check=False
        )
        ssim_match = re.search(r'All:(\d+\.\d+)', ssim_result.stderr)
        if ssim_match:
            ssim_val = float(ssim_match.group(1))
            measures.average(video_file, 'video_perf', 'ssim').record(ssim_val)
            logging.info("SSIM: %f", ssim_val)
        else:
            logging.warning(
                "Failed to parse SSIM from FFmpeg output. Stderr: %s",
                ssim_result.stderr,
            )
    except Exception as e:  # pylint: disable=broad-exception-caught
        logging.error("Failed to calculate SSIM: %s", e)


def finalize_results(chrome_version=None):
    """Dumps metrics and uploads to ResultDB if available."""
    if chrome_version:
        # Tag results with the chrome version for easier tracking in dashboards.
        measures.tag(chrome_version)

    # Dump metrics to the expected location for the result_adapter or
    # other infra tools to find.
    logging.info("Dumping metrics to: %s", perf_config.INVOCATIONS_DIR)
    measures.dump(perf_config.INVOCATIONS_DIR)

    # If running in a LUCI environment, try to upload immediately.
    client = result_sink.TryInitClient()
    if client:
        logging.info("LUCI ResultSink detected. Uploading extended properties.")
        try:
            records = {measures.TEST_SCRIPT_METRICS_KEY: measures.to_dict()}
            client.UpdateInvocationExtendedProperties(records)
            logging.info("Metrics uploaded successfully.")
        except Exception as e:  # pylint: disable=broad-exception-caught
            logging.error("Failed to upload metrics to ResultSink: %s", e)


def _parse_float(val):
    """Parses a float value from a string, ignoring formatting."""
    if not val:
        return None
    match = re.search(r'[-+]?\d+(?:\.\d+)?', val)
    return float(match.group(0)) if match else None


def _record_cros_power(video_file, csv_local_path):
    """Records the average of a ChromeOS power log (one value per line)."""
    power_draws = []
    with open(csv_local_path, mode='r', encoding='utf-8') as f:
        for line in f:
            val = line.strip()
            if val:
                power_draws.append(float(val))

    if not power_draws:
        return
    avg_raw = sum(power_draws) / len(power_draws)
    # Auto-scale: if the raw value is very large, it is in microwatts.
    if avg_raw > 10000:
        avg_power = avg_raw / 1000000.0
    elif avg_raw > 10:
        avg_power = avg_raw / 1000.0
    else:
        avg_power = avg_raw

    measures.average(
        video_file, 'video_perf', 'power_consumption_watts'
    ).record(avg_power)
    logging.info("ChromeOS Average Power Draw: %.2f W", avg_power)


def _record_glances_csv(video_file, csv_local_path):
    """Records average CPU and battery power from a glances CSV export."""
    cpu_usages = []
    power_draws = []
    with open(csv_local_path, mode='r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            cpu_val = _parse_float(row.get('cpu.total'))
            if cpu_val is not None:
                cpu_usages.append(cpu_val)

            power_val = _parse_float(row.get('sensors.Battery.value'))
            if power_val is not None:
                power_draws.append(power_val)

    if cpu_usages:
        avg_cpu = sum(cpu_usages) / len(cpu_usages)
        measures.average(video_file, 'video_perf', 'cpu_utilization').record(
            avg_cpu
        )
        logging.info("Average CPU utilization: %.2f%%", avg_cpu)

    if power_draws:
        avg_power = sum(power_draws) / len(power_draws)
        measures.average(
            video_file, 'video_perf', 'power_consumption_watts'
        ).record(avg_power)
        logging.info("Average Power Draw: %.2f W", avg_power)


def parse_glances_csv_and_record(video_file, csv_local_path, sender_os):
    """Parses the glances/power log and records metrics.

    Args:
        video_file: The name of the video file.
        csv_local_path: The local path to the CSV/log file.
        sender_os: The OS of the sender device.
    """
    if not os.path.exists(csv_local_path):
        logging.warning(
            "Monitoring log file not found at %s. Skipping metric parsing.",
            csv_local_path,
        )
        return

    try:
        if sender_os == 'cros':
            _record_cros_power(video_file, csv_local_path)
        else:
            _record_glances_csv(video_file, csv_local_path)
    except Exception as e:  # pylint: disable=broad-exception-caught
        logging.error("Failed to parse monitoring log: %s", e)
